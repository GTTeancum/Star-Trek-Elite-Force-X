// Directly include the renderer implementation. All D3D objects, files and
// signature handles below are in-memory test doubles, not emulated hardware.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using BYTE=unsigned char;
using DWORD=uint32_t;
using BOOL=int;
using HRESULT=int32_t;
using HANDLE=void *;
const BOOL TRUE=1, FALSE=0;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define SUCCEEDED(h) ((h) >= 0)
#define D3DCOLOR(v) (v)
enum { D3DBACKBUFFER_TYPE_MONO, D3DFMT_DXT1, D3DX_DEFAULT, FILE_BEGIN,
       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, GENERIC_READ=8, GENERIC_WRITE=16 };
enum { ERROR_SUCCESS=0, ERROR_FILE_NOT_FOUND=2, ERROR_PATH_NOT_FOUND=3, ERROR_ACCESS_DENIED=5 };
enum { CSS_IMAGE_HDR_SIZE=2048, SAVE_GAME_IMAGE_W=256,SAVE_GAME_IMAGE_H=128, XCALCSIG_FLAG_SAVE_GAME=0 };
const DWORD XPR_MAGIC_VALUE=0x30525058;
struct XPR_HEADER { DWORD dwMagic,dwTotalSize,dwHeaderSize; };
struct XCALCSIG_SIGNATURE { DWORD v[5]; };
struct RECT {int left,top,right,bottom;}; struct D3DSURFACE_DESC {unsigned Width,Height;};
using Bytes=std::vector<BYTE>;
static std::map<std::string,Bytes> files;
static const std::string largePath="Z:\\screenshot.xbx", iconPath="Z:\\saveimage.xbx",
 defaultPath="d:\\base\\media\\defaultsaveimage.xbx", dstIcon="slot/icon", dstLarge="slot/large";
static std::string fault, selectedPath;
static unsigned generation=1,width=640,height=480,surfaces=0;
static int opens=0,hashes=0,conversions=0,exports=0,creates=0,deleted=0;
static DWORD lastError=0;
static std::vector<RECT> crops;
static bool fail(const std::string &stage,const std::string &path){
 return stage==fault && (selectedPath.empty() || path==selectedPath);
}
struct Surface {
 unsigned w,h; std::string path;bool owned;
 Surface(unsigned w_,unsigned h_,const std::string &p):w(w_),h(h_),path(p),owned(true){++surfaces;}
 void Release(){assert(owned&&surfaces);owned=false;--surfaces;delete this;}
 HRESULT GetDesc(D3DSURFACE_DESC *d) {assert(owned);d->Width=w;d->Height=h;return fault=="desc-error"?-1:0;}
};
using LPDIRECT3DSURFACE8=Surface*;
struct Device {
 HRESULT GetBackBuffer(int k,int mode,Surface **out) {
  assert(k==0&&mode==D3DBACKBUFFER_TYPE_MONO&&!*out);
  if(fault=="source-null")return 0;
  if(fault=="source-error")return -1;
  *out=new Surface(width,height,"backbuffer");return fault=="source-error-owned"?-1:0;
 }
 HRESULT CreateImageSurface(unsigned w,unsigned h,int fmt,Surface **out) {
  assert(!*out && fmt==D3DFMT_DXT1);++creates;const std::string &p=w==64?iconPath:largePath;
  if(fail("dest-null",p))return 0;
  if(fail("dest-error",p))return -1;
  *out=new Surface(w,h,p);return fail("dest-error-owned",p)?-1:0;
 }
}; using LPDIRECT3DDEVICE8=Device*;
HRESULT D3DXLoadSurfaceFromSurface(Surface *d,void*,void*,Surface *s,void*,const RECT *r,int,unsigned){
 assert(d&&d->owned&&s&&s->owned&&s->path=="backbuffer");
 assert(r->left>=0&&r->top>=0&&r->right>r->left&&r->bottom>r->top&&
        unsigned(r->right)<=s->w&&unsigned(r->bottom)<=s->h);
 crops.push_back(*r);++conversions;return fail("convert",d->path)?-1:0;
}
static Bytes rawXpr(unsigned w,unsigned h){
 Bytes result(CSS_IMAGE_HDR_SIZE+w*h/2,0); XPR_HEADER hdr={XPR_MAGIC_VALUE,DWORD(result.size()),CSS_IMAGE_HDR_SIZE};
 memcpy(result.data(),&hdr,sizeof(hdr));
 for(size_t i=CSS_IMAGE_HDR_SIZE;i<result.size();++i)result[i]=BYTE(generation+17u*i);
 return result;
}
HRESULT XGWriteSurfaceOrTextureToXPR(Surface *s,const char *p,BOOL asTexture){
 assert(s&&s->owned&&s->path==p&&asTexture);++exports;Bytes data=rawXpr(s->w,s->h);
 if(fail("partial-export",p))data.resize(41);
 if(fail("bad-header",p))data[0]^=255;
 files[p]=data; return fail("export-error",p)?-1:0;
}
DWORD GetLastError(){return lastError;}
BOOL DeleteFile(const char *p){
 if(fail("deny-remove",p)){lastError=ERROR_ACCESS_DENIED;return FALSE;}
 if(files.erase(p)){++deleted;return TRUE;}lastError=ERROR_FILE_NOT_FOUND;return FALSE;
}
BOOL CopyFile(const char *a,const char *b,BOOL failIfExists){
 assert(!failIfExists);
 if(files.count(a) && !fail("copy",a)){files[b]=files[a];return TRUE;}
 return FALSE;
}
struct File { std::string p; DWORD pos; };
HANDLE CreateFile(const char *p,int access,int share,void*,int how,int,void*){
 assert(how==OPEN_EXISTING && share==0 && access==(GENERIC_READ|(p==largePath?GENERIC_WRITE:0)));
 if(!files.count(p)||fail("open",p))return INVALID_HANDLE_VALUE;
 ++opens;return new File{p,0};
}
DWORD GetFileSize(HANDLE h,DWORD *hi){auto f=(File*)h;*hi=fail("oversize",f->p)?1:0;return files.at(f->p).size();}
DWORD SetFilePointer(HANDLE h,int n,void*,int origin){
 auto f=(File*)h;assert(n==0&&origin==FILE_BEGIN);f->pos=0;return fail("seek",f->p)?0xffffffffu:0;
}
BOOL ReadFile(HANDLE h,void *p,DWORD n,DWORD *done,void*) {
 auto f=(File*)h;auto &bytes=files.at(f->p);bool header=(n==sizeof(XPR_HEADER));
 const char *hard=header?"header-error":"read-error", *partial=header?"header-short":"read-short";
 if(fail(hard,f->p)){*done=0;return FALSE;}
 *done=std::min<DWORD>(bytes.size()-f->pos, n);if(fail(partial,f->p)&&*done)--*done;
 memcpy(p,bytes.data()+f->pos,*done);f->pos+=*done;return TRUE;
}
BOOL WriteFile(HANDLE h,const void *p,DWORD n,DWORD *done,void*) {
 auto f=(File*)h;auto &bytes=files.at(f->p);assert(f->pos==bytes.size()&&n==sizeof(XCALCSIG_SIGNATURE));
 if(fail("write-error",f->p)){*done=0;return FALSE;}
 *done=n-fail("write-short",f->p);auto src=(const BYTE*)p;bytes.insert(bytes.end(),src,src+*done);return TRUE;
}
BOOL CloseHandle(HANDLE h){auto f=(File*)h;bool ok=!fail("close",f->p);delete f;assert(opens>0);--opens;return ok;}
static XCALCSIG_SIGNATURE digest(const Bytes &b){
 XCALCSIG_SIGNATURE h={{1,2,3,4,5}};for(size_t i=0;i<b.size();++i)h.v[i%5]=h.v[i%5]*65537u+b[i];return h;
}
HANDLE XCalculateSignatureBegin(int flags){assert(flags==XCALCSIG_FLAG_SAVE_GAME);
 if(fault=="hash-null")return nullptr;
 if(fault=="hash-begin")return INVALID_HANDLE_VALUE;
 ++hashes;return new Bytes;
}
DWORD XCalculateSignatureUpdate(HANDLE h,const BYTE *p,DWORD n){assert(h&&h!=INVALID_HANDLE_VALUE);
 ((Bytes*)h)->insert(((Bytes*)h)->end(),p,p+n);return fault=="hash-update"?1:ERROR_SUCCESS;}
DWORD XCalculateSignatureEnd(HANDLE h,XCALCSIG_SIGNATURE *sig){assert(hashes>0);--hashes;
 *sig=digest(*(Bytes*)h);delete (Bytes*)h;return fault=="hash-end"?1:ERROR_SUCCESS;}

#include "../../../code/win32/xbox_save_preview.inl"

static Device device;
static void capture(Device *d=&device,bool third=false){
 creates=conversions=exports=0;crops.clear();XB_CaptureSavePreviews(d,third);
 assert(surfaces==0 && opens==0 && hashes==0);
}
static void copy(){XB_CopyCurrentSavePreviews(dstIcon.c_str(),dstLarge.c_str());}
static void validateLarge(){
 auto raw=rawXpr(256,128);auto sig=digest(raw);
 raw.insert(raw.end(),(BYTE*)&sig,(BYTE*)&sig+sizeof(sig));assert(files.at(dstLarge)==raw);
}
static void validateIcon(){assert(files.at(dstIcon)==rawXpr(64,64));}
static unsigned cases=0;
int main(){
 const Bytes fallback={127,87,13};files[defaultPath]=fallback;
 files[largePath]={92};files[iconPath]={93};copy();
 assert(files.at(dstIcon)==fallback&&files.at(dstLarge)==fallback);++cases;
 for(unsigned h:{480u,720u,120u}){
  height=h;width=(h==480?640u:h==720?1280u:160u);
  for(bool third:{false,true}){++generation;capture(&device,third);assert(creates==2&&conversions==2&&exports==2);
   copy();validateLarge();validateIcon();assert(crops.size()==2);
   assert(crops[0].top==int(height)*3/40&&crops[1].left==std::max(0,int(width)/2-120));
   if(h>=480) { assert(crops[1].top==int(h)/2-120+(third?60:0)); } ++cases;}
 }
 width=640;height=480;
 for(const char *which:{"device","source-null","source-error","source-error-owned","desc-error","dimensions"}){
  ++generation;fault=which;if(fault=="dimensions")width=0;
  capture(fault=="device"?nullptr:&device);assert(!creates&&!conversions&&!exports);copy();
  assert(files.at(dstLarge)==fallback&&files.at(dstIcon)==fallback);++cases;width=640;
 }
 for(const auto &p:{largePath,iconPath}){
  for(const char *stage:{"deny-remove","dest-null","dest-error","dest-error-owned","convert","export-error","partial-export","bad-header",
                         "oversize","open","header-short","header-error","close","copy"}){
   fault.clear();selectedPath.clear();++generation;capture();copy();validateLarge();validateIcon();
   ++generation;fault=stage;selectedPath=p;capture();copy();
   if(p==largePath){assert(files.at(dstLarge)==fallback);validateIcon();}
   else{assert(files.at(dstIcon)==fallback);validateLarge();}
   ++cases;
  }
 }
 selectedPath=largePath;
 for(const char *stage:{"seek","read-error","read-short","hash-null","hash-begin","hash-update","hash-end","write-error","write-short"}){
  ++generation;fault=stage;capture();copy();assert(files.at(dstLarge)==fallback);validateIcon();++cases;
 }
 // If a build has no packaged fallback, don't publish another generation's
 // scratch, or leave a partially copied preview. Never delete the payload.
 files.erase(defaultPath);files["slot/JK3SG.xsv"]={7,8,9};fault="source-error";selectedPath="";
 capture();copy();assert(!files.count(dstIcon)&&!files.count(dstLarge)&&files.at("slot/JK3SG.xsv")==Bytes({7,8,9}));++cases;
 fault.clear();++generation;capture();copy();validateIcon();validateLarge();++cases;
 printf("PASS %u ordinary preview cases: legacy formats/crops, independently failed D3D and file/signature operations, stale scratch/default selection, cleanup and successful retry.\n",cases);
}
