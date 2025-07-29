// Demo: How to use the Billboard System with Sprite Sheet Animation
// Place this in your Application.cpp to see animated sprites in action

// In the playground section, replace the billboard creation with this:

// Create animated billboards using sprite sheets
billboards.resize(3);

// Billboard 1: Static sprite (using full texture)
billboards[0] = Az3D::Billboard(glm::vec3(2.0f, 1.0f, 0.0f), 1.0f, 1.0f, mapTextureIndex);
billboards[0].uvMin = glm::vec2(0.0f, 0.0f);  // Full texture
billboards[0].uvMax = glm::vec2(1.0f, 1.0f);

// Billboard 2: Animated sprite (4x4 sprite sheet simulation)
billboards[1] = Az3D::Billboard(glm::vec3(-2.0f, 1.5f, 0.0f), 0.8f, 0.8f, playerTextureIndex);
// Start with first frame (top-left quarter)
billboards[1].uvMin = glm::vec2(0.0f, 0.0f);
billboards[1].uvMax = glm::vec2(0.5f, 0.5f);

// Billboard 3: Another animated sprite with different timing
billboards[2] = Az3D::Billboard(glm::vec3(0.0f, 2.0f, -2.0f), 1.2f, 1.2f, mapTextureIndex);
// Start with second frame (top-right quarter)
billboards[2].uvMin = glm::vec2(0.5f, 0.0f);
billboards[2].uvMax = glm::vec2(1.0f, 0.5f);

// Then in the main loop (after the model update section), add this animation code:

// Animate sprite sheets
static float spriteTimer = 0.0f;
spriteTimer += dTime;

// Update every 200ms (5 FPS animation)
if (spriteTimer >= 0.2f) {
    // Simulate 4-frame animation for billboard 1
    static int frame1 = 0;
    frame1 = (frame1 + 1) % 4;
    
    // Calculate UV coordinates for 2x2 grid
    float col = frame1 % 2;
    float row = frame1 / 2;
    
    billboards[1].uvMin = glm::vec2(col * 0.5f, row * 0.5f);
    billboards[1].uvMax = glm::vec2((col + 1) * 0.5f, (row + 1) * 0.5f);
    
    // Animate billboard 2 with different pattern
    static int frame2 = 0;
    frame2 = (frame2 + 1) % 4;
    
    // Different animation pattern (reverse order)
    int reverseFrame = 3 - frame2;
    col = reverseFrame % 2;
    row = reverseFrame / 2;
    
    billboards[2].uvMin = glm::vec2(col * 0.5f, row * 0.5f);
    billboards[2].uvMax = glm::vec2((col + 1) * 0.5f, (row + 1) * 0.5f);
    
    spriteTimer = 0.0f;
}

// For more complex sprite sheets (8x8, 16x16), use this pattern:
/*
// Example: 8x8 sprite sheet (64 frames)
int totalFrames = 64;
int gridSize = 8;
int currentFrame = (int)(totalTime * animationSpeed) % totalFrames;

int col = currentFrame % gridSize;
int row = currentFrame / gridSize;

float cellWidth = 1.0f / gridSize;
float cellHeight = 1.0f / gridSize;

billboard.uvMin = glm::vec2(col * cellWidth, row * cellHeight);
billboard.uvMax = glm::vec2((col + 1) * cellWidth, (row + 1) * cellHeight);
*/
