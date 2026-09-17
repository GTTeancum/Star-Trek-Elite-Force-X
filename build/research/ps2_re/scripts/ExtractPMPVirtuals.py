#@category STEFX
import json
from ghidra.app.decompiler import DecompInterface
args=getScriptArgs()
roots=[0x20adf8,0x20af20,0x20b158,0x20b2c0,0x20b368,0x20afe0,0x20b610,0x20b780,0x20b790,0x20b598,0x2834e8]
functions={}
for address in roots:
    fun=getFunctionAt(toAddr(address))
    if fun:
        functions[str(fun.getEntryPoint())]=fun
for fun in list(functions.values()):
    for callee in fun.getCalledFunctions(monitor):
        functions[str(callee.getEntryPoint())]=callee
result={'language':str(currentProgram.getLanguageID()),'analysis_limit':'Prior automatic analysis timed out; this export does not claim full analysis completion','functions':[]}
decomp=DecompInterface()
decomp.openProgram(currentProgram)
for key,fun in sorted(functions.items()):
    print('PMP_EXPORT_BEGIN: '+key)
    res=decomp.decompileFunction(fun,20,monitor)
    c=res.getDecompiledFunction()
    instructions=[]
    for ins in currentProgram.getListing().getInstructions(fun.getBody(),True):
        instructions.append(str(ins.getAddress())+' '+str(ins))
    result['functions'].append({'address':key,'name':fun.getName(),'c':c.getC() if c else res.getErrorMessage(),'assembly':instructions,'calls':[str(x.getEntryPoint()) for x in fun.getCalledFunctions(monitor)]})
    with open(args[0],'w') as stream:
        json.dump(result,stream,indent=2)
decomp.dispose()
print('PMP_EXPORT_DONE: %d functions' % len(result['functions']))
