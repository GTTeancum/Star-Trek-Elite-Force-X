#@category STEFX
"""Export functions that reference renderer configuration strings.

The result is a focused, reproducible call neighbourhood for comparing the
PS2 renderer with the Xbox renderer.  Ghidra's generated C remains provisional;
the included assembly is the authority for implementation decisions.
"""
import json
from ghidra.app.decompiler import DecompInterface


TARGETS = set([
    "r_lodCurveError", "r_lodbias", "r_flares", "r_fastsky", "r_drawSun",
    "r_dynamiclight", "r_dlightBacks", "r_facePlaneCull", "r_nocurves",
    "r_drawworld", "r_drawentities", "r_nocull", "r_novis", "r_vertexLight",
    "r_subdivisions", "r_lodscale", "r_portalOnly", "r_skipBackEnd",
    "r_finish", "r_lightmap", "r_textureMode", "r_swapInterval"
    , "R_GetCommandBuffer: bad size %i"
    , "R_GetCommandBuffer: ran out of room. %d of %d"
    , "change SWAP_DRAW_SURF macro"
    , "R_AddEntitySurfaces: Bad modeltype"
    , "R_AddEntitySurfaces: Bad reType"
    , "Bad index in triangle surface"
    , "Bad surfaceType"
])

args = getScriptArgs()
output_path = args[0]
listing = currentProgram.getListing()
references = currentProgram.getReferenceManager()
manager = currentProgram.getFunctionManager()

strings = []
roots = {}
data_iter = listing.getDefinedData(True)
while data_iter.hasNext():
    data = data_iter.next()
    if not data.hasStringValue():
        continue
    value = str(data.getValue())
    if value not in TARGETS:
        continue
    row = {"value": value, "address": str(data.getAddress()), "references": []}
    for ref in references.getReferencesTo(data.getAddress()):
        source = ref.getFromAddress()
        function = manager.getFunctionContaining(source)
        row["references"].append({
            "from": str(source),
            "type": str(ref.getReferenceType()),
            "function": str(function.getEntryPoint()) if function else None
        })
        if function:
            roots[str(function.getEntryPoint())] = function
    strings.append(row)

# Include one call level in both directions.  This is intentionally bounded so
# generic cvar registration helpers do not pull most of the executable in.
functions = dict(roots)
for function in list(roots.values()):
    for related in function.getCalledFunctions(monitor):
        functions[str(related.getEntryPoint())] = related
    for related in function.getCallingFunctions(monitor):
        functions[str(related.getEntryPoint())] = related

decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
exported = []
for key, function in sorted(functions.items()):
    result = decompiler.decompileFunction(function, 30, monitor)
    decompiled = result.getDecompiledFunction()
    instructions = []
    for instruction in listing.getInstructions(function.getBody(), True):
        instructions.append(str(instruction.getAddress()) + " " + str(instruction))
    exported.append({
        "address": key,
        "name": function.getName(),
        "root": key in roots,
        "c": decompiled.getC() if decompiled else result.getErrorMessage(),
        "assembly": instructions,
        "calls": [str(item.getEntryPoint()) for item in function.getCalledFunctions(monitor)],
        "called_by": [str(item.getEntryPoint()) for item in function.getCallingFunctions(monitor)]
    })
decompiler.dispose()

payload = {
    "language": str(currentProgram.getLanguageID()),
    "targets": sorted(TARGETS),
    "strings": sorted(strings, key=lambda row: (row["value"], row["address"])),
    "root_function_count": len(roots),
    "functions": exported,
    "limitations": [
        "Generated C is provisional and must be checked against assembly.",
        "The call neighbourhood is limited to one level around direct string references.",
        "A missing string reference may reflect an indirect address construction or incomplete analysis."
    ]
}
with open(output_path, "w") as stream:
    json.dump(payload, stream, indent=2)
print("RENDERER_EXPORT_DONE: %d strings, %d roots, %d functions" %
      (len(strings), len(roots), len(exported)))
