mkdir Build -p && cd Build && cmake .. && make -j12 && cd .. 
glslc compute.comp -o compute.comp.spv
