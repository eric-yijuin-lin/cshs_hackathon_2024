import os
from pathlib import Path

from PIL import Image

from torchvision.datasets import ImageFolder
import torchvision.transforms as transforms
import torch
import torchvision.models as models
import torch.nn as nn
import torch.nn.functional as F



def get_default_device():
    """Pick GPU if available, else CPU"""
    if torch.cuda.is_available():
        return torch.device('cuda')
    else:
        return torch.device('cpu')
    
def to_device(data, device):
    """Move tensor(s) to chosen device"""
    if isinstance(data, (list,tuple)):
        return [to_device(x, device) for x in data]
    return data.to(device, non_blocking=True)

def accuracy(outputs, labels):
    _, preds = torch.max(outputs, dim=1)
    return torch.tensor(torch.sum(preds == labels).item() / len(preds))



defualt_device = get_default_device()
data_dir  = 'D:/Coding/AI/Common Datasets/garbage_dataset/Garbage classification/Garbage classification'
classes = os.listdir(data_dir)
print(classes)
transformations = transforms.Compose([transforms.Resize((256, 256)), transforms.ToTensor()])
dataset = ImageFolder(data_dir, transform = transformations)



class ImageClassificationBase(nn.Module):
    def training_step(self, batch):
        images, labels = batch 
        out = self(images)                  # Generate predictions
        loss = F.cross_entropy(out, labels) # Calculate loss
        return loss
    
    def validation_step(self, batch):
        images, labels = batch 
        out = self(images)                    # Generate predictions
        loss = F.cross_entropy(out, labels)   # Calculate loss
        acc = accuracy(out, labels)           # Calculate accuracy
        return {'val_loss': loss.detach(), 'val_acc': acc}
        
    def validation_epoch_end(self, outputs):
        batch_losses = [x['val_loss'] for x in outputs]
        epoch_loss = torch.stack(batch_losses).mean()   # Combine losses
        batch_accs = [x['val_acc'] for x in outputs]
        epoch_acc = torch.stack(batch_accs).mean()      # Combine accuracies
        return {'val_loss': epoch_loss.item(), 'val_acc': epoch_acc.item()}
    
    def epoch_end(self, epoch, result):
        print("Epoch {}: train_loss: {:.4f}, val_loss: {:.4f}, val_acc: {:.4f}".format(
            epoch+1, result['train_loss'], result['val_loss'], result['val_acc']))

class ResNet(ImageClassificationBase):
    def __init__(self):
        super().__init__()
        # Use a pretrained model
        self.network = models.resnet50(pretrained=True)
        # Replace last layer
        num_ftrs = self.network.fc.in_features
        self.network.fc = nn.Linear(num_ftrs, len(dataset.classes))
    
    def forward(self, xb):
        return torch.sigmoid(self.network(xb))

cpu_device = torch.device('cpu')
gpu_model = ResNet()
cpu_model = ResNet()
gpu_model.load_state_dict(torch.load("./ai/test_model.pt"), torch.device('cuda'))
cpu_model.load_state_dict(torch.load("./ai/test_model.pt", map_location=cpu_device))

def predict_image(img, model, device=defualt_device):
    # Convert to a batch of 1
    xb = to_device(img.unsqueeze(0), device)
    # Get predictions from model
    yb = model(xb)
    # Pick index with highest probability
    prob, preds  = torch.max(yb, dim=1)
    # Retrieve the class label
    return dataset.classes[preds[0].item()]

def predict_external_image(image_name):
    image = Image.open(Path('./' + image_name))
    example_image = transformations(image)
    # plt.imshow(example_image.permute(1, 2, 0))
    image_class = predict_image(example_image, cpu_model, device=cpu_device)
    print(f"The image resembles {image_class}.")
    return image_class
    # print("The image resembles", predict_image(example_image, gpu_model, device=defualt_device) + ".")

# predict_external_image('D:/Coding/AI/Common Datasets/garbage_dataset/Garbage classification/Garbage classification/plastic/plastic51.jpg')