from pathlib import Path
import html, re
root = Path(__file__).resolve().parent

def inline(t):
    t = html.escape(t)
    t = re.sub(r'\[([^\]]+)\]\((https?://[^\s)]+)\)',r'<a href="\2">\1</a>',t)
    t = re.sub(r'\*\*(.+?)\*\*',r'<strong>\1</strong>',t)
    return re.sub(r'`([^`]+)`',r'<code>\1</code>',t)

parts=[]
blocks=root.joinpath('RESEARCH.md').read_text().split('\n\n')
for b in blocks:
    b=b.strip()
    if not b: continue
    if b.startswith('# '): parts.append('<h1>'+inline(b[2:])+'</h1>')
    elif b.startswith('## '): parts.append('<h2>'+inline(b[3:])+'</h2>')
    elif b.startswith('|'):
        rows=b.splitlines(); cells=lambda row: row.strip('|').split('|')
        parts.append('<div class="table"><table><thead><tr>'+''.join('<th>'+inline(c.strip())+'</th>' for c in cells(rows[0]))+'</tr></thead><tbody>')
        for row in rows[2:]: parts.append('<tr>'+''.join('<td>'+inline(c.strip())+'</td>' for c in cells(row))+'</tr>')
        parts.append('</tbody></table></div>')
    elif re.match(r'\d+\. ',b):
        parts.append('<ol>'+''.join('<li>'+inline(re.sub(r'^\d+\. ','',l))+'</li>' for l in b.splitlines())+'</ol>')
    else: parts.append('<p>'+inline(b)+'</p>')
parts.append('<h2>Reference sources</h2><p>The research document links to external source materials; this guide does not embed copies.</p>')
style='''body{max-width:980px;margin:64px auto;padding:0 28px;background:#f4f2ec;color:#232b26;font:17px/1.7 system-ui}h1{font-size:46px;letter-spacing:-1.6px;line-height:1.08;max-width:800px}h2{font-size:27px;line-height:1.2;margin-top:52px;border-top:1px solid #c5c9bf;padding-top:24px}a{color:#21603b;text-underline-offset:3px}code{background:#e6e7df;padding:2px 5px;border-radius:3px;font-size:.9em}.table{overflow:auto}table{border-collapse:collapse;font-size:14px;line-height:1.5}td,th{text-align:left;border-bottom:1px solid #c5c9bf;padding:12px 10px}th{background:#e3e8db}figure{margin:24px 0}summary{cursor:pointer;font-weight:650;padding:14px;background:#e3e8db}img{max-width:100%;height:auto}figcaption{font-size:13px;color:#596354}li{margin-bottom:12px}@media(max-width:650px){body{margin:30px auto;padding:0 20px}h1{font-size:34px}}'''
doc='<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Resonant Four — circuit research</title><style>'+style+'</style></head><body><main>'+''.join(parts)+'</main></body></html>'
root.joinpath('RESEARCH.html').write_text(doc)
print('Rendered research guide:',len(doc),'bytes')
