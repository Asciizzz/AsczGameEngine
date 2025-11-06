# Game Setup Guide

This guide explains how to set up the shooting game with player, enemies, and bullets.

## Scripts Created

1. **game.lua** - Main game manager (handles enemy spawning, game state)
2. **player.lua** - Player controller (movement, shooting, health)
3. **enemy.lua** - Enemy AI (chases player with smooth rotation)
4. **bullet.lua** - Bullet behavior (moves and damages enemies)

## Scene Hierarchy Setup

```
GameManager (Node with game.lua script)
├── Player (Node with player.lua script)
│   └── [Optional: Animation Node]
├── EnemyContainer (Empty Node)
│   └── [Enemies will spawn here]
└── BulletContainer (Empty Node)
    └── [Bullets will spawn here]
```

## Step-by-Step Setup

### 1. Create the Game Manager Node

1. Create a new node called "GameManager"
2. Attach `game.lua` script to it
3. Configure the following variables in the script inspector:
   - `play` = false (set to true to start the game)
   - `fieldMinX`, `fieldMaxX`, `fieldMinZ`, `fieldMaxZ` = playing field bounds
   - `enemySpawnY` = height where enemies spawn (typically 0)
   - `spawnRate` = starting spawn rate (2.0 seconds recommended)

### 2. Create the Player Node

1. Create a player node (can be a 3D model)
2. Attach `player.lua` script
3. Make it a child of GameManager (or reference it in GameManager's `playerNode` handle)
4. Configure player variables:
   - `vel` = movement speed (2.0 recommended)
   - `hp` = starting health (100.0)
   - `shotRate` = time between shots (0.2 seconds)
   - `shotDamage` = damage per bullet (10.0)
   - `shotSpeed` = bullet speed (10.0)
   - `animeNode` = reference to animation node (optional)

### 3. Create Container Nodes

1. Create "EnemyContainer" (empty node)
2. Create "BulletContainer" (empty node)
3. Both should be children of GameManager

### 4. Create Enemy Scene/Prefab

1. Create a new scene/prefab for enemies
2. Should have a 3D model or visual representation
3. Attach `enemy.lua` script
4. Configure enemy variables:
   - `hp` = enemy health (30.0)
   - `speed` = movement speed (3.0)
   - `rotationSpeed` = rotation smoothness (3.0)
   - `contactDamage` = damage to player (5.0)
   - `damageRate` = damage per second (1.0)
5. Save this as a reusable scene

### 5. Create Bullet Scene/Prefab

1. Create a new scene/prefab for bullets
2. Should have a small 3D model (sphere/cylinder)
3. Attach `bullet.lua` script
4. Configure bullet variables:
   - `lifetime` = how long before auto-delete (5.0 seconds)
5. Save this as a reusable scene

### 6. Link Everything Together

#### On GameManager (game.lua):
- `playerNode` → Player node reference
- `enemyContainerNode` → EnemyContainer node reference
- `bulletContainerNode` → BulletContainer node reference
- `enemyScene` → Enemy scene/prefab reference
- `bulletScene` → Bullet scene/prefab reference

#### On Player (player.lua):
- `bulletContainerNode` → BulletContainer node reference
- `bulletScene` → Bullet scene/prefab reference
- `animeNode` → Animation node reference (optional)

#### On Bullet (bullet.lua):
- `enemyContainerNode` → EnemyContainer node reference
- (Other vars like dirX, dirZ, speed, damage are set by player at spawn)

#### On Enemy (enemy.lua):
- `playerNode` → Set automatically by game.lua at spawn

## How to Play

1. Set `play = true` on the GameManager script
2. Use arrow keys to move
3. Press spacebar to shoot
4. Enemies spawn from edges and chase you
5. Survive as long as possible!

## Controls

- **Arrow Keys** - Move player
- **Shift** - Run (faster movement)
- **Spacebar** - Shoot

## Game Mechanics

- **Enemy Spawning**: Enemies spawn from random edges of the play field
- **Spawn Rate**: Increases over time (spawns faster as game progresses)
- **Enemy AI**: Enemies smoothly rotate and converge towards player (not straight line)
- **Collision**: Enemies damage player on contact
- **Bullets**: Destroy enemies on hit, auto-delete after 5 seconds
- **Death**: Game ends when player HP reaches 0

## Difficulty Tuning

Adjust these values for different difficulty:

**Easier:**
- Increase `player.hp` (more health)
- Decrease `enemy.speed` (slower enemies)
- Decrease `enemy.contactDamage` (less damage)
- Increase `game.spawnRate` (slower spawning)

**Harder:**
- Decrease `player.hp` (less health)
- Increase `enemy.speed` (faster enemies)
- Increase `enemy.contactDamage` (more damage)
- Decrease `game.spawnRate` (faster spawning)
- Decrease `game.spawnRateMin` (higher max difficulty)

## Troubleshooting

**Enemies not spawning:**
- Check that `enemyScene` and `enemyContainerNode` are set on GameManager
- Make sure `play = true`

**Player can't shoot:**
- Check that `bulletScene` and `bulletContainerNode` are set on Player
- Verify bullet scene has the bullet.lua script attached

**Bullets don't hit enemies:**
- Check that `enemyContainerNode` is set on the bullet scene/prefab
- Verify enemies are children of the EnemyContainer node

**Enemy doesn't chase player:**
- The `playerNode` reference is set automatically by game.lua when spawning
- If manually placing enemies, set `playerNode` reference on enemy script
