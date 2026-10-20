from pathlib import Path
import subprocess,tempfile,hashlib,json,sys
root=Path(sys.argv[1] if len(sys.argv)>1 else '/home/yann/projects/lithtech/sources/blood2')
p=root/'ClientShellDLL/Music.cpp';source=p.read_text()
def function(name):
 start=source.index('DBOOL CMusic::'+name+'(');brace=source.index('{',start);level=1;i=brace+1
 while level:
  level+=(source[i]=='{')-(source[i]=='}');i+=1
 return source[start:i].replace('CMusic::','ReferenceMusic::')
arrays='\n'.join(source[source.index('static char '+name):source.index(';',source.index('static char '+name))+1] for name in ['g_szAmbientTrans','g_szCruisingTrans','g_szHarddrivingTrans'])
prefix=r'''
#include <cstdint>
#include <iostream>
#include <string>
using DBOOL=bool;
#define DTRUE true
#define DFALSE false
#define DNULL nullptr
#define PARSE_MAXTOKENSIZE 80
#define MUSIC_IMMEDIATE 0
#define MUSIC_NEXTSONG 2
struct Client {std::string list,transition;bool loop=false;int boundary=-1;
bool PlayList(const char* l,const char* t,bool r,int b){list=l;transition=t?t:"";loop=r;boundary=b;return true;}};
class ReferenceMusic {public:
enum EMusicLevel:unsigned { MUSICLEVEL_SILENCE, MUSICLEVEL_AMBIENT, MUSICLEVEL_CRUISING, MUSICLEVEL_HARDDRIVING };
Client* m_pClientDE;bool m_bUseIma=true;EMusicLevel m_eMusicLevel;
bool IsPlaylistInitialized(){return true;}
bool PlayMusicLevel(EMusicLevel);
bool TransitionToLevel(char*,char*,EMusicLevel,bool);
};
'''
suffix=r'''
int main(){std::uint64_t hash=14695981039346656037ull;unsigned requests=0;
auto byte=[&](unsigned b){hash^=b;hash*=1099511628211ull;};
auto text=[&](const std::string&s){for(unsigned char c:s)byte(c);byte(0);};
for(unsigned from=0;from<256;++from)for(unsigned to=0;to<256;++to){
 Client client;ReferenceMusic m;m.m_pClientDE=&client;m.m_eMusicLevel=ReferenceMusic::EMusicLevel(from);
 bool ok=m.PlayMusicLevel(ReferenceMusic::EMusicLevel(to));byte(from);byte(to);byte(ok);
 if(ok){++requests;text(client.list);text(client.transition);byte(client.boundary==MUSIC_IMMEDIATE);byte(client.loop);}}
 std::cout<<hash<<" "<<requests<<"\n";}
'''
with tempfile.TemporaryDirectory(prefix='b2-playlist-reference-') as tmp:
 cpp=Path(tmp)/'reference.cpp';exe=Path(tmp)/'reference';cpp.write_text(prefix+arrays+function('PlayMusicLevel')+function('TransitionToLevel')+suffix)
 cmd=['c++','-std=c++20','-Wno-write-strings','-O0',str(cpp),'-o',str(exe)]
 subprocess.check_call(cmd);result=subprocess.check_output([str(exe)],text=True).split()
print(json.dumps({'source':'sources/blood2/ClientShellDLL/Music.cpp','source_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'cases':65536,'accepted_requests':int(result[1]),'decision_fnv1a64':result[0],'scope':'Unchanged original PlayMusicLevel and TransitionToLevel decision branches; initialized IMA, recorded PlayList sink returns true. No audio/engine call success inferred.','compiler':subprocess.check_output(['c++','--version'],text=True).splitlines()[0]},indent=2))
