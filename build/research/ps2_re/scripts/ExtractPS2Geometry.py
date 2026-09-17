#@category STEFX
import json
from ghidra.app.decompiler import DecompInterface
args=getScriptArgs()
roots=[0x209a68,0x209aa8,0x209be0,0x20d708,0x20d970,0x20dbc0,0x20ded8,0x20e138,0x20e1e8,0x20e278,0x20f038]
functions={}
for address in roots:
    fun=getFunctionAt(toAddr(address))
    if fun:
        functions[str(fun.getEntryPoint())]=fun
for fun in list(functions.values()):
    for callee in fun.getCalledFunctions(monitor):
        functions[str(callee.getEntryPoint())]=callee
result={'language':str(currentProgram.getLanguageID()),'analysis_limit':'Consult geometry_analysis.log for analysis completion; decompiler output still requires assembly validation','functions':[]}
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
