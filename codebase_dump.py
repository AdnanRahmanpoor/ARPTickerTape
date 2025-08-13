import os
import argparse

def generate_codebase_dump(root_dir, output_file, excluded_dirs=None, excluded_extensions=None):
    """
    Generates a consolidated file containing all codebase files with their paths
    
    :param root_dir: Root directory to scan
    :param output_file: Output file name
    :param excluded_dirs: Directories to exclude
    :param excluded_extensions: File extensions to exclude
    """
    if excluded_dirs is None:
        excluded_dirs = {'venv', '.env', '.git', '__pycache__', 'node_modules'}
    
    if excluded_extensions is None:
        excluded_extensions = {'.png', '.jpg', '.jpeg', '.gif', '.pdf', '.bin', '.DS_Store'}

    with open(output_file, 'w', encoding='utf-8') as outfile:
        for root, dirs, files in os.walk(root_dir):
            # Skip excluded directories
            dirs[:] = [d for d in dirs if d not in excluded_dirs]
            
            for file in files:
                file_path = os.path.join(root, file)
                rel_path = os.path.relpath(file_path, start=root_dir)
                
                # Skip excluded extensions and hidden files
                if (os.path.splitext(file)[1] in excluded_extensions or 
                    file.startswith('.')):
                    continue
                
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='replace') as infile:
                        content = infile.read()
                        
                        # Write file header
                        outfile.write(f"\n\n{'='*80}\n")
                        outfile.write(f"FILE: {rel_path}\n")
                        outfile.write(f"{'='*80}\n\n")
                        
                        # Write line numbers and content
                        for i, line in enumerate(content.split('\n'), 1):
                            outfile.write(f"{i:04d} | {line}\n")
                            
                except Exception as e:
                    print(f"Error processing {file_path}: {str(e)}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate codebase dump')
    parser.add_argument('root_dir', help='Root directory to scan')
    parser.add_argument('output_file', help='Output file name')
    args = parser.parse_args()

    generate_codebase_dump(
        root_dir=args.root_dir,
        output_file=args.output_file
    )