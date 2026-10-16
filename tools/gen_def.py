# Gera o .def de uma DLL proxy: cada exportacao da DLL original vira um encaminhamento para <nome>_orig.dll.
# uso: python gen_def.py caminho\da\dll_original.dll saida.def
import sys, os, pefile
src, out = sys.argv[1], sys.argv[2]
base = os.path.splitext(os.path.basename(src))[0]
pe = pefile.PE(src, fast_load=True); pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
lines = ['LIBRARY "%s.dll"' % base, 'EXPORTS']
for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if s.name: lines.append('    %s=%s_orig.%s @%d' % (s.name.decode(), base, s.name.decode(), s.ordinal))
    else: lines.append('    ord%d=%s_orig.#%d @%d NONAME' % (s.ordinal, base, s.ordinal, s.ordinal))
open(out, 'w', newline='\n').write('\n'.join(lines) + '\n')
print(len(lines) - 2, 'exportacoes')
