#@category STEFX
import json
from ghidra.app.decompiler import DecompInterface
args=getScriptArgs()
targets=json.load(open(args[0]))
result=[]
functions={}
for target in targets:
    refs=[]
    for ref in getReferencesTo(toAddr(target['address'])):
        source=ref.getFromAddress()
        fun=getFunctionContaining(source)
        refs.append({'from':str(source),'function':str(fun.getEntryPoint()) if fun else None})
        if fun: functions[str(fun.getEntryPoint())]=fun
    target['references']=refs
    result.append(target)
# Expand only loader/allocation/map roots into their direct callees.
for address in [0x1fd050,0x1fd088,0x1fd208,0x1fd2d8,0x20adc8,0x20b398,0x20b5b8,0x20b868,0x20b658,0x201630,0x202510,0x1ffef8,0x2009f0,0x20f038,0x2834e8]:
    fun=getFunctionAt(toAddr(address))
    if fun: functions[str(fun.getEntryPoint())]=fun
roots=list(functions.values())
for fun in roots:
    for callee in fun.getCalledFunctions(monitor):
        functions[str(callee.getEntryPoint())]=callee
decomp=DecompInterface()
decomp.openProgram(currentProgram)
outputs=[]
for key,fun in sorted(functions.items()):
    print("STEFX_RE_BEGIN: " + key)
    res=decomp.decompileFunction(fun,15,monitor)
    c=res.getDecompiledFunction()
    outputs.append({'address':key,'name':fun.getName(),'c':c.getC() if c else res.getErrorMessage(),'calls':[str(x.getEntryPoint()) for x in fun.getCalledFunctions(monitor)]})
decomp.dispose()
json.dump({'language':str(currentProgram.getLanguageID()),'targets':result,'functions':outputs},open(args[1],'w'),indent=2)
print('STEFX_RE: %d referenced functions decompiled' % len(outputs))
