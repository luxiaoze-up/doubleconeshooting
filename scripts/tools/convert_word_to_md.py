import os
import glob
from docx import Document
from docx.document import Document as _Document
from docx.oxml.text.paragraph import CT_P
from docx.oxml.table import CT_Tbl
from docx.table import _Cell, Table
from docx.text.paragraph import Paragraph

def iter_block_items(parent):
    """
    Yield each paragraph and table child within *parent*, in document order.
    Each returned value is an instance of either Table or Paragraph.
    """
    if isinstance(parent, _Document):
        parent_elm = parent.element.body
    elif isinstance(parent, _Cell):
        parent_elm = parent._tc
    else:
        raise ValueError("something's not right")

    for child in parent_elm.iterchildren():
        if isinstance(child, CT_P):
            yield Paragraph(child, parent)
        elif isinstance(child, CT_Tbl):
            yield Table(child, parent)

def convert_docx_to_md(source_dir, output_dir):
    # Find all .docx files
    docx_files = glob.glob(os.path.join(source_dir, '**', '*.docx'), recursive=True)

    if not docx_files:
        print(f"No .docx files found in {source_dir}")
    else:
        print(f"Found {len(docx_files)} .docx files.")

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    for file_path in docx_files:
        try:
            print(f"Processing {file_path}...")
            doc = Document(file_path)
            
            md_content = f"# File: {os.path.basename(file_path)}\n\n"
            md_content += f"Source: `{file_path}`\n\n"

            for block in iter_block_items(doc):
                if isinstance(block, Paragraph):
                    text = block.text.strip()
                    if text:
                        # Simple heuristic for headers based on style name
                        style_name = block.style.name
                        # print(f"DEBUG: style_name='{style_name}', text='{text[:20]}'") # Uncomment for debugging
                        
                        if 'Heading 1' in style_name or '标题 1' in style_name or '1级标题' in style_name:
                            md_content += f"# {text}\n\n"
                        elif 'Heading 2' in style_name or '标题 2' in style_name or '2级标题' in style_name:
                            md_content += f"## {text}\n\n"
                        elif 'Heading 3' in style_name or '标题 3' in style_name or '3级标题' in style_name:
                            md_content += f"### {text}\n\n"
                        elif 'Heading 4' in style_name or '标题 4' in style_name or '4级标题' in style_name:
                            md_content += f"#### {text}\n\n"
                        elif 'Title' in style_name or '标题' == style_name: # Exact match for "标题" sometimes used for Title
                            md_content += f"# {text}\n\n"
                        else:
                            md_content += f"{text}\n\n"
                
                elif isinstance(block, Table):
                    # Convert table to markdown
                    rows = block.rows
                    if not rows:
                        continue
                    
                    # Extract data
                    table_data = []
                    for row in rows:
                        row_data = [cell.text.strip().replace('\n', ' ') for cell in row.cells]
                        table_data.append(row_data)
                    
                    if not table_data:
                        continue

                    # Create markdown table
                    headers = table_data[0]
                    # Ensure headers are not empty for markdown table validity if possible, 
                    # but markdown tables are flexible.
                    
                    # Header row
                    md_content += "| " + " | ".join(headers) + " |\n"
                    # Separator row
                    md_content += "| " + " | ".join(["---"] * len(headers)) + " |\n"
                    
                    # Data rows
                    for row in table_data[1:]:
                        md_content += "| " + " | ".join(row) + " |\n"
                    
                    md_content += "\n"

            # Output file
            base_name = os.path.splitext(os.path.basename(file_path))[0]
            output_file = os.path.join(output_dir, base_name + ".md")
            
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(md_content)
            
            print(f"Saved to {output_file}")

        except Exception as e:
            print(f"Error processing {file_path}: {e}")

    # Check for .doc files and warn
    doc_files = glob.glob(os.path.join(source_dir, '**', '*.doc'), recursive=True)
    if doc_files:
        print("\nWARNING: The following .doc files were found but cannot be processed directly (convert to .docx first):")
        for f in doc_files:
            print(f" - {f}")

if __name__ == "__main__":
    # 从 scripts/tools/ 向上两级到项目根目录
    workspace_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    source_directory = os.path.join(workspace_root, "reference", "设计文档")
    output_directory = os.path.join(workspace_root, "docs", "word_exports")
    
    print(f"Scanning {source_directory}...")
    convert_docx_to_md(source_directory, output_directory)
