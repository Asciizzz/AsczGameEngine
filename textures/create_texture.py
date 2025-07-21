from PIL import Image
import numpy as np

# Create a 512x512 checkerboard pattern
size = 512
checkerboard = np.zeros((size, size, 3), dtype=np.uint8)

# Create checkerboard pattern
square_size = 64
for i in range(size):
    for j in range(size):
        if ((i // square_size) + (j // square_size)) % 2 == 0:
            checkerboard[i, j] = [255, 255, 255]  # White
        else:
            checkerboard[i, j] = [255, 0, 0]      # Red

# Save as JPEG
img = Image.fromarray(checkerboard)
img.save('texture.jpg', 'JPEG', quality=95)
print("Created texture.jpg - a red and white checkerboard pattern")
