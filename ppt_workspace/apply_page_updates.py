import json
import subprocess
import os

# 读取更新列表
with open('page_updates.json', 'r', encoding='utf-8') as f:
    updates = json.load(f)

PID = "DPUosmcpDl71W4d3xCKcgwGqnXc"

# 只处理需要更新的（页码变化的）
to_update = [u for u in updates if u['old_text'] != u['new_text']]
print(f"Need to update {len(to_update)} slides")

results = []
for i, u in enumerate(to_update):
    slide_id = u['slide_id']
    shape_id = u['shape_id']
    new_text = u['new_text']
    
    # 构建parts JSON
    parts = [
        {
            "action": "block_replace",
            "block_id": shape_id,
            "replacement": f'<shape type="text" topLeftX="820" topLeftY="505" width="95" height="20"><content textType="caption" fontSize="11" fontFamily="Noto Sans CJK SC" color="rgba(100,116,139,1)" textAlign="right"><p>{new_text}</p></content></shape>'
        }
    ]
    
    # 写入parts文件
    parts_file = f"parts_{i}.json"
    with open(parts_file, 'w', encoding='utf-8') as f:
        json.dump(parts, f, ensure_ascii=False)
    
    # 调用命令
    cmd = f'lark-cli slides +replace-slide --presentation {PID} --slide-id {slide_id} --parts "@{parts_file}" --json'
    print(f"[{i+1}/{len(to_update)}] Updating slide {slide_id} page number to {new_text}...")
    
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=os.getcwd())
    
    if result.returncode == 0:
        print(f"  Success")
        results.append({'slide_id': slide_id, 'success': True})
    else:
        print(f"  Failed: {result.stderr[:200]}")
        results.append({'slide_id': slide_id, 'success': False, 'error': result.stderr[:200]})

print(f"\nDone. Success: {sum(1 for r in results if r['success'])}/{len(results)}")
