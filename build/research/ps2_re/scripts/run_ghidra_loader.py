from pathlib import Path
import subprocess
root=Path.cwd();base=root/'build/research/ps2_re'
a=[r'C:\Program Files\Eclipse Adoptium\jdk-21.0.10.7-hotspot\bin\java.exe','-Xmx2G','-Duser.home='+str(base/'tool_home'),'-XX:ParallelGCThreads=2','-XX:CICompilerCount=2','-Djava.system.class.loader=ghidra.GhidraClassLoader','-Dfile.encoding=UTF-8','-cp',r'Z:\Programming\ghidra_11.0.1_PUBLIC\Ghidra\Framework\Utility\lib\Utility.jar','ghidra.Ghidra','ghidra.app.util.headless.AnalyzeHeadless',str(base/'ghidra'),'stefx_ps2_r5900','-processor','r5900:LE:32:default','-cspec','default','-import',str(base/'SLUS_202.27'),'-analysisTimeoutPerFile','180','-scriptPath',str(base/'scripts'),'-postScript','ExtractLoader.py',str(base/'re_targets.json'),str(base/'loader_decompilation.json')]
with (base/'ghidra_loader.log').open('w') as f: result=subprocess.run(a,stdout=f,stderr=subprocess.STDOUT)
raise SystemExit(result.returncode)
