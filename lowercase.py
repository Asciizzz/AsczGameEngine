
import os

def rename_tiny_to_tiny(root_dir):
    # Walk the directory tree from bottom up to avoid issues with renaming folders
    for dirpath, dirnames, filenames in os.walk(root_dir, topdown=False):
        # Rename files
        for filename in filenames:
            if "Tiny" in filename:
                old_path = os.path.join(dirpath, filename)
                new_filename = filename.replace("Tiny", "tiny")
                new_path = os.path.join(dirpath, new_filename)
                os.rename(old_path, new_path)
                print(f"Renamed file: {old_path} -> {new_path}")
        # Rename directories
        for dirname in dirnames:
            if "Tiny" in dirname:
                old_dir = os.path.join(dirpath, dirname)
                new_dirname = dirname.replace("Tiny", "tiny")
                new_dir = os.path.join(dirpath, new_dirname)
                os.rename(old_dir, new_dir)
                print(f"Renamed folder: {old_dir} -> {new_dir}")

if __name__ == "__main__":
    # Change this to your workspace root if needed
    root_directory = os.path.dirname(os.path.abspath(__file__))
    rename_tiny_to_tiny(root_directory)