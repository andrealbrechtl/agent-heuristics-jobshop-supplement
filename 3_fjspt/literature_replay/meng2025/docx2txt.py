import sys, zipfile, re, html
z=zipfile.ZipFile(sys.argv[1])
x=z.read('word/document.xml').decode('utf8')
paras=re.findall(r'<w:p[ >].*?</w:p>|<w:p/>', x, re.S)
out=[]
for p in paras:
    toks=re.findall(r'<w:t[^>]*>(.*?)</w:t>|<w:(br|cr|tab)\b[^>]*/>', p, re.S)
    s=''.join(html.unescape(t) if t else ('\t' if k=='tab' else '\n') for t,k in toks)
    out.append(s)
open(sys.argv[2],'w').write('\n'.join(out))
