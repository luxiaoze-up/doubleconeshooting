import os
import pandas as pd
import glob

def convert_excel_to_md(source_dir, output_dir):
    # Find all Excel files
    excel_files = glob.glob(os.path.join(source_dir, '**', '*.xlsx'), recursive=True)
    excel_files += glob.glob(os.path.join(source_dir, '**', '*.xls'), recursive=True)

    if not excel_files:
        print(f"No Excel files found in {source_dir}")
        return

    print(f"Found {len(excel_files)} Excel files.")

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    for file_path in excel_files:
        try:
            print(f"Processing {file_path}...")
            # Read all sheets
            xls = pd.ExcelFile(file_path)
            
            # Create a markdown string
            md_content = f"# File: {os.path.basename(file_path)}\n\n"
            md_content += f"Source: `{file_path}`\n\n"

            for sheet_name in xls.sheet_names:
                md_content += f"## Sheet: {sheet_name}\n\n"
                df = pd.read_excel(xls, sheet_name=sheet_name)
                
                # Convert to markdown
                # fillna('') replaces NaN with empty string for cleaner tables
                markdown_table = df.fillna('').to_markdown(index=False)
                md_content += markdown_table + "\n\n"

            # Determine output filename
            # Maintain some directory structure or just flatten? 
            # Let's flatten for now but prepend parent folder name to avoid collisions if needed, 
            # or just use the filename.
            base_name = os.path.splitext(os.path.basename(file_path))[0]
            output_file = os.path.join(output_dir, base_name + ".md")
            
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(md_content)
            
            print(f"Saved to {output_file}")

        except Exception as e:
            print(f"Error processing {file_path}: {e}")

if __name__ == "__main__":
    # 从 scripts/tools/ 向上两级到项目根目录
    workspace_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    source_directory = os.path.join(workspace_root, "reference", "设计文档")
    output_directory = os.path.join(workspace_root, "docs", "excel_exports")
    
    print(f"Scanning {source_directory}...")
    convert_excel_to_md(source_directory, output_directory)
