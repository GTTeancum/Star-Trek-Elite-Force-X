from pathlib import Path
import shutil
root=Path.cwd();backup=root/'build/research/ps2_re/index_stream_candidate';backup.mkdir(exist_ok=True)
paths=[root/'code/renderer/tr_bsp_xbox.cpp',root/'code/qcommon/cm_load_xbox.cpp']
for path in paths:
    target=backup/(path.name+'.before')
    if target.exists():raise SystemExit('Existing backup: inspect before applying again')
texts=[p.read_text() for p in paths]
r,c=texts
# Add optional index streaming to internal parsers while keeping ordinary callers unchanged.
def once(text,old,new):
    assert text.count(old)==1,(old[:100],text.count(old))
    return text.replace(old,new,1)
r=once(r,'void *surfaces, int surfacelen ) {\n\tdtrisurf_t','void *surfaces, int surfacelen, LumpStream *indexStream = NULL,\n                    short *indexScratch = NULL ) {\n\tdtrisurf_t')
r=once(r,'void *surfaces, int surfacelen, LumpStream *surfaceStream ) {','void *surfaces, int surfacelen, LumpStream *surfaceStream,\n                    LumpStream *indexStream = NULL, short *indexScratch = NULL ) {')
for kind in ['TriSurf','Face']:
    old='\t\tParseTriSurf( parseSurface, parseVerts, out, indexes );' if kind=='TriSurf' else '\t\tParseFace(parseSurface, parseVerts, out, indexes, faceDataCursor, resolvedShader, faceDataEnd);'
    call='\t\tParseTriSurf(parseSurface, parseVerts, out, parseIndexes);' if kind=='TriSurf' else '\t\tParseFace(parseSurface, parseVerts, out, parseIndexes, faceDataCursor, resolvedShader, faceDataEnd);'
    new='''\t\tshort *parseIndexes = indexes;
        if (indexStream)
        {
            const unsigned int packed = parseSurface->indexes;
            const int count = packed & 0xFFF;
            if (!indexScratch || !indexStream->readAt((packed >> 12) * sizeof(short),
                    indexScratch, count * sizeof(short)))
            {
                Com_Error(ERR_DROP, "STEFX: packed index read failed at surface %d", i);
                return;
            }
            localSurface = *parseSurface;
            localSurface.indexes = count;
            parseSurface = &localSurface;
            parseIndexes = indexScratch;
        }
'''+call
    r=once(r,old,new)
r=once(r,'''void R_LoadTriSurfsStream( void *indexdata, int indexlen, LumpStream *verts,
					void *surfaces, int surfacelen ) {
	R_LoadTriSurfsInternal(indexdata, indexlen, NULL, verts ? verts->len : 0,
		verts, surfaces, surfacelen);
}''','''void R_LoadTriSurfsStream( LumpStream *indexes, short *indexScratch, LumpStream *verts,
                    void *surfaces, int surfacelen ) {
    R_LoadTriSurfsInternal(NULL, indexes ? indexes->len : 0, NULL, verts ? verts->len : 0,
        verts, surfaces, surfacelen, indexes, indexScratch);
}''')
r=once(r,'''void R_LoadFacesStream( void *indexdata, int indexlen, LumpStream *verts,
					LumpStream *surfaces ) {
	R_LoadFacesInternal(indexdata, indexlen, NULL, verts ? verts->len : 0,
		verts, NULL, surfaces ? surfaces->len : 0, surfaces);
}''','''void R_LoadFacesStream( LumpStream *indexes, short *indexScratch, LumpStream *verts,
                    LumpStream *surfaces ) {
    R_LoadFacesInternal(NULL, indexes ? indexes->len : 0, NULL, verts ? verts->len : 0,
        verts, NULL, surfaces ? surfaces->len : 0, surfaces, indexes, indexScratch);
}''')
c=once(c,'void R_LoadTriSurfsStream( void *indexdata, int indexlen,','void R_LoadTriSurfsStream( LumpStream *indexes, short *indexScratch,')
c=once(c,'void R_LoadFacesStream( void *indexdata, int indexlen,','void R_LoadFacesStream( LumpStream *indexes, short *indexScratch,')
c=once(c,'''\t\tLump indexes;
		g_SPXBPackedMapPhase = 0x504D0014;
		indexes.load(stripName, "indexes");''','''\t\tLumpStream indexes;
        // Packed surface counts use 12 bits: 8 KiB covers every index range.
        g_SPXBPackedMapPhase = 0x504D0014;
        if (!indexes.open(stripName, "indexes") || (indexes.len & 1))
        {
            Com_Error(ERR_DROP, "CM_LoadMap: invalid packed index stream for %s", name);
        }
        short *indexScratch = (short *)Z_Malloc(8192, TAG_TEMP_WORKSPACE, qfalse, 32);
        XBLog_WriteCriticalf("STEFX_INDEX_STREAM: map='%s' fileBytes=%d scratchBytes=8192 begin",
            name, indexes.len);''')
c=once(c,'R_LoadTriSurfsStream(indexes.data, indexes.len,','R_LoadTriSurfsStream(&indexes, indexScratch,')
c=once(c,'R_LoadFacesStream(indexes.data, indexes.len,','R_LoadFacesStream(&indexes, indexScratch,')
c=once(c,'''\t\tfaces.close();
		CM_EFRecordPackedPhase(4);''','''\t\tfaces.close();
        XBLog_WriteCriticalf("STEFX_INDEX_STREAM: map='%s' fileBytes=%d scratchBytes=8192 done",
            name, indexes.len);
        indexes.close();
        Z_Free(indexScratch);
        CM_EFRecordPackedPhase(4);''')
for path,text in zip(paths,[r,c]):
    shutil.copy2(path,backup/(path.name+'.before'))
    with path.open('w',newline='') as f:f.write(text.replace('\n','\r\n'))
print('Updated packed index reads in two files; original parser paths retained. Source candidate is unqualified.')
