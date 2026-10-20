from pathlib import Path
import subprocess
base=Path('/tmp/b2-model-prep-baseline');(base/'engine').mkdir(parents=True,exist_ok=True)
for name in ['src/engine/model.cpp','src/engine/model_view.cpp','src/engine/model_view.h','src/app/model_scene.cpp']:
 out=base/('engine/model_view.h' if name.endswith('.h') else Path(name).name)
 out.write_bytes(subprocess.check_output(['git','show','2c82c1fc2c744bced9847dc66b9c4b7b51f1bd93:'+name]))
s=Path('/tmp/b2-container-index-equivalence.cpp').read_text()
start=s.index('double ms(');end=s.index('struct StateDigest',start);s=s[:start]+s[end:]
needle=' bool frame(GameSession&g)'
method=''' void view(const ModelView&v){add(v.mesh.vertices.size());for(const auto&p:v.mesh.vertices){vec(p.position);vec(p.color);add(p.uv[0]);add(p.uv[1]);}add(v.mesh.ranges.size());for(const auto&r:v.mesh.ranges){add(r.first);add(r.count);for(const auto&b:r.batches){add(b.first);add(b.count);add(b.material);add(b.translucent);}}for(const auto* draws:{&v.world_draws,&v.close_draws}){add(draws->size());for(const auto&d:*draws){add(d.range);vec(d.position);vec(d.rotation.m_Vec);add(d.rotation.m_Spin);add(d.alpha);}}for(const auto&e:v.entries){add(e.object.slot);add(e.object.generation);add(e.visible);add(e.close);add(e.range);add(e.triangles);add(e.hidden_triangles);add(e.hidden_nodes);add(e.alpha);bytes(e.skin.data(),e.skin.size());bytes(e.error.data(),e.error.size());}}
''';s=s.replace(needle,method+needle)
a=s.index('bool run(GameSession&g)');b=s.index('int main(',a)
s=s[:a]+'''bool run(GameSession&g){std::string error;ModelView models,effects;StateDigest digest;for(int i=0;i<360;++i){if(!g.step(1./60)||!update_model_scene(g,models,error)||!update_explosion_model_scene(g,effects,error)||!digest.frame(g)){std::cerr<<error;return false;}digest.view(models);digest.view(effects);}std::cout<<"DIGEST "<<g.world_path<<" "<<digest.value<<"\\n";return true;}
'''+s[b:]
Path('/tmp/b2-model-prep-equivalence.cpp').write_text(s)
