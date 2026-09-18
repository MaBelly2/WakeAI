import xml.etree.ElementTree as ET
import re
import os

# 解析XML
tree = ET.parse('full-latest.xml')
root = tree.getroot()

# 命名空间
ns = {'ns': 'https://www.larkoffice.com/sml/2.0'}
ET.register_namespace('', 'https://www.larkoffice.com/sml/2.0')

# 获取所有slide
slides = root.findall('.//ns:slide', ns)
print(f"Total slides: {len(slides)}")

# 遍历每个slide，找到页码块
total = len(slides)
updates = []

for i, slide in enumerate(slides):
    slide_id = slide.get('id')
    page_num = i + 1
    
    # 找到所有文本shape
    shapes = slide.findall('.//ns:shape', ns)
    for shape in shapes:
        shape_id = shape.get('id')
        topLeftX = float(shape.get('topLeftX', 0))
        topLeftY = float(shape.get('topLeftY', 0))
        
        # 页码块的特征：右下角，x≈820, y≈505
        if topLeftX > 800 and topLeftY > 490:
            content = shape.find('ns:content', ns)
            if content is not None:
                p = content.find('ns:p', ns)
                if p is not None and p.text:
                    old_text = p.text.strip()
                    # 检查是否是页码格式
                    if re.match(r'\d+\s*/\s*\d+', old_text):
                        new_text = f"{page_num:02d} / {total:02d}"
                        print(f"Slide {page_num} ({slide_id}): '{old_text}' -> '{new_text}'")
                        updates.append({
                            'slide_id': slide_id,
                            'shape_id': shape_id,
                            'old_text': old_text,
                            'new_text': new_text
                        })

print(f"\nTotal updates needed: {len(updates)}")

# 保存更新列表
import json
with open('page_updates.json', 'w', encoding='utf-8') as f:
    json.dump(updates, f, ensure_ascii=False, indent=2)
