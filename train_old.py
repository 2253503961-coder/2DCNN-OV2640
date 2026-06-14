import torch
import torch.nn as nn
import torch.optim as optim
from torchvision import datasets, transforms
from torch.utils.data import DataLoader, random_split
import datetime
import os

class TinyVisionCNN(nn.Module):
    def __init__(self, num_classes=4):
        super(TinyVisionCNN, self).__init__()
        # 输入: 1通道(灰度图) x 28 x 28
        self.conv1 = nn.Conv2d(in_channels=1, out_channels=4, kernel_size=3, padding=1)
        self.relu1 = nn.ReLU()
        self.pool1 = nn.MaxPool2d(kernel_size=2, stride=2)
        # 输出: 4通道 x 14 x 14

        self.conv2 = nn.Conv2d(in_channels=4, out_channels=8, kernel_size=3, padding=1)
        self.relu2 = nn.ReLU()
        self.pool2 = nn.MaxPool2d(kernel_size=2, stride=2)
        # 输出: 8通道 x 7 x 7

        # 展平后的特征数: 8 * 7 * 7 = 392
        self.fc = nn.Linear(392, num_classes)

    def forward(self, x):
        x = self.pool1(self.relu1(self.conv1(x)))
        x = self.pool2(self.relu2(self.conv2(x)))
        x = torch.flatten(x, 1)
        x = self.fc(x)
        return x

# ==========================================
# 2. 训练与导出流程
# ==========================================
def main():
    # 数据预处理：转为灰度图 -> 缩小到 28x28 -> 转为张量 -> 归一化 [-1, 1]
    transform = transforms.Compose([
        transforms.Grayscale(num_output_channels=1),
        transforms.Resize((28, 28)),
        transforms.ToTensor(),
        transforms.Normalize((0.5,), (0.5,))
    ])

    # 加载数据集 (文件夹名称会自动被解析为标签)
    dataset_path = 'dataset'
    if not os.path.exists(dataset_path):
        print(f"❌ 找不到文件夹 '{dataset_path}'，请检查路径！")
        return

    full_dataset = datasets.ImageFolder(root=dataset_path, transform=transform)
    class_names = full_dataset.classes
    num_classes = len(class_names)
    print(f"📦 发现类别: {class_names}")

    # 划分训练集和验证集 (80% 训练，20% 验证)
    train_size = int(0.8 * len(full_dataset))
    val_size = len(full_dataset) - train_size
    train_dataset, val_dataset = random_split(full_dataset, [train_size, val_size])

    train_loader = DataLoader(train_dataset, batch_size=16, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=16, shuffle=False)

    # 初始化模型
    model = TinyVisionCNN(num_classes=num_classes)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=0.002)

    # 开始训练
    epochs = 70
    print(f"\n开始训练 {num_classes} 分类视觉模型...")
    for epoch in range(epochs):
        model.train()
        total_loss, correct, total = 0, 0, 0
        
        for inputs, labels in train_loader:
            optimizer.zero_grad()
            outputs = model(inputs)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()

            total_loss += loss.item()
            _, predicted = torch.max(outputs.data, 1)
            total += labels.size(0)
            correct += (predicted == labels).sum().item()

        # 验证集评估
        model.eval()
        val_correct, val_total = 0, 0
        with torch.no_grad():
            for val_inputs, val_labels in val_loader:
                val_outputs = model(val_inputs)
                _, val_predicted = torch.max(val_outputs.data, 1)
                val_total += val_labels.size(0)
                val_correct += (val_predicted == val_labels).sum().item()

        print(f"Epoch [{epoch+1}/{epochs}] Loss: {total_loss/len(train_loader):.4f} "
              f"| Train Acc: {100 * correct / total:.2f}% | Val Acc: {100 * val_correct / val_total:.2f}%")
# ==========================================
    # 3. 导出模型 (ONNX + C 头文件)
    # ==========================================
    model.eval() # 切换到评估模式 (非常重要，防止 Dropout/BatchNorm 影响导出)
    
    # -----------------------------------
    # 阶段 A: 导出为 ONNX 格式
    # -----------------------------------
    print("\n📦 正在导出 ONNX 模型...")
    onnx_file_path = "gesture_vision.onnx"
    # STM32 需要一个明确尺寸的假输入来推断图结构 (BatchSize设为1, 1通道, 28x28)
    dummy_input = torch.randn(1, 1, 28, 28) 
    
    torch.onnx.export(
        model,                       # 要导出的模型
        dummy_input,                 # 模型的虚拟输入
        onnx_file_path,              # 保存路径
        export_params=True,          # 将训练好的权重存入模型文件内
        opset_version=11,            # ONNX 算子集版本 (11 兼容性较好)
        do_constant_folding=True,    # 是否执行常量折叠优化
        input_names=['input'],       # 输入节点的名称
        output_names=['output'],     # 输出节点的名称
    )
    print(f"✅ 成功生成 ONNX 模型: {onnx_file_path}")
    print("   (提示: 此文件可直接导入 STM32Cube.AI 自动生成推理代码)")

    print("\n📦 正在生成 C 语言权重头文件...")
    h_file_path = "cnn_vision_weights.h"
    with open(h_file_path, "w") as f:
        f.write("/*\n * 自动生成的超轻量级 2D CNN 权重 (专为单片机视觉设计)\n")
        f.write(f" * 生成时间: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f" * 类别映射: ")
        for i, name in enumerate(class_names):
            f.write(f"[{i}]: {name} ")
        f.write("\n * 输入尺寸: 1 x 28 x 28 (单通道灰度图)\n */\n\n")
        f.write("#ifndef CNN_VISION_WEIGHTS_H\n#define CNN_VISION_WEIGHTS_H\n\n")

        # 遍历模型参数，展平并写入 C 数组
        for name, param in model.named_parameters():
            flat_data = param.data.numpy().flatten()
            array_str = ", ".join([f"{x:.6f}f" for x in flat_data])
            # 将 Python 命名 (如 conv1.weight) 转为合法的 C 变量名 (如 conv1_weight)
            var_name = name.replace(".", "_")
            f.write(f"const float {var_name}[{len(flat_data)}] = {{{array_str}}};\n\n")
            
        f.write("#endif // CNN_VISION_WEIGHTS_H\n")
    print(f"✅ 成功生成 C 头文件: {h_file_path}")
    print("   (提示: 此文件用于纯手写推理代码或调用 CMSIS-NN 库)")

if __name__ == "__main__":
    main()