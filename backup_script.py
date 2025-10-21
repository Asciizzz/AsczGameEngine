#!/usr/bin/env python3
"""
AsczGameEngine Project Backup Script
Creates timestamped backups of essential project files and folders.

Usage: python backup_project.py
"""

import os
import shutil
import datetime
from pathlib import Path

def create_backup():
    # Get current date in AYYMMDD format (A = last digit of year)
    now = datetime.datetime.now()
    year_last_2_digit = now.year % 100
    date_str = f"A{year_last_2_digit}{now.month:02d}{now.day:02d}"
    
    # Define paths
    project_root = Path(__file__).parent
    backup_dir = project_root / ".backup" / date_str
    
    # Essential files and folders to backup
    essential_items = [
        "include/",      # Header files
        "src/",          # Source code  
        "main.cpp",      # Main entry point
        "Shaders/",      # Shader files
        "Config/",       # Configuration files
        ".gitignore",    # Git ignore rules
        "CMakeLists.txt",# CMake build configuration
        "ReadMe.md",     # Documentation
        "backup_script.py" # Sure, you can come along
    ]
    
    print(f"🚀 AsczGameEngine Backup Tool")
    print(f"📅 Creating backup: {date_str}")
    print(f"📂 Backup location: {backup_dir}")
    print("-" * 50)
    
    # Create backup directory
    backup_dir.mkdir(parents=True, exist_ok=True)
    
    # Copy each essential item
    for item in essential_items:
        source_path = project_root / item
        dest_path = backup_dir / item
        
        if source_path.exists():
            if source_path.is_file():
                # Copy file
                dest_path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source_path, dest_path)
                print(f"✅ Copied file: {item}")
            elif source_path.is_dir():
                # Copy directory
                if dest_path.exists():
                    shutil.rmtree(dest_path)
                shutil.copytree(source_path, dest_path)
                
                # Count files in directory
                file_count = sum(1 for _ in dest_path.rglob('*') if _.is_file())
                print(f"✅ Copied folder: {item} ({file_count} files)")
        else:
            print(f"⚠️  Not found: {item}")
    
    print("-" * 50)
    
    # Calculate backup size
    total_size = 0
    file_count = 0
    for root, dirs, files in os.walk(backup_dir):
        for file in files:
            file_path = Path(root) / file
            total_size += file_path.stat().st_size
            file_count += 1
    
    # Convert size to human readable
    size_mb = total_size / (1024 * 1024)
    
    print(f"📊 Backup Statistics:")
    print(f"   • Total files: {file_count}")
    print(f"   • Total size: {size_mb:.2f} MB")
    print(f"   • Location: {backup_dir}")
    
    # Check if this backup already exists
    existing_backups = list((project_root / ".backup").glob("*"))
    if len(existing_backups) > 1:
        print(f"📚 You have {len(existing_backups)} backups total")
    
    print("\n🎉 Backup completed successfully!")
    
    return backup_dir

def list_backups():
    """List all existing backups"""
    project_root = Path(__file__).parent
    backup_root = project_root / ".backup"
    
    if not backup_root.exists():
        print("📁 No backups found")
        return
    
    backups = sorted(backup_root.glob("*"))
    if not backups:
        print("📁 No backups found")
        return
    
    print(f"📚 Available backups:")
    for backup in backups:
        if backup.is_dir():
            # Parse date from folder name (AYYMMDD format)
            date_str = backup.name
            if len(date_str) == 5 and date_str.isdigit():
                year_digit = date_str[0]
                month = date_str[1:3]
                day = date_str[3:5]
                
                # Assume 2020s for year (5 = 2025, 6 = 2026, etc.)
                year = f"202{year_digit}"
                
                try:
                    parsed_date = datetime.datetime.strptime(f"{year}-{month}-{day}", "%Y-%m-%d")
                    formatted_date = parsed_date.strftime("%B %d, %Y")
                    
                    # Get backup size
                    total_size = 0
                    for root, dirs, files in os.walk(backup):
                        for file in files:
                            file_path = Path(root) / file
                            total_size += file_path.stat().st_size
                    size_mb = total_size / (1024 * 1024)
                    
                    print(f"   📅 {date_str} - {formatted_date} ({size_mb:.2f} MB)")
                except ValueError:
                    print(f"   📅 {date_str}")
            else:
                print(f"   📅 {date_str}")

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == "list":
        list_backups()
    else:
        create_backup()
        
        # Also show existing backups
        print()
        list_backups()