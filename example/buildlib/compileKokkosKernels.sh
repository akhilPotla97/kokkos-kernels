KOKKOS_PATH=${HOME}/work/kokkos #path to kokkos source
KOKKOSKERNELS_SCALARS=double #the scalar types to instantiate =double,float...
KOKKOSKERNELS_LAYOUTS=LayoutLeft,LayoutRight #the layout types to instantiate.
KOKKOSKERNELS_ORDINALS=int,int64_t #ordinal types to instantiate
KOKKOSKERNELS_OFFSETS=int,size_t #offset types to instantiate
KOKKOSKERNELS_PATH=../.. #path to kokkos-kernels top directory.
CXX=icpc #
KOKKOSKERNELS_INSTALL_PATH=./install #path where kokkos-kernels will be installed to
KOKKOSKERNELS_OPTIONS=eti-only #options for kokkoskernels  
KOKKOS_DEVICES=OpenMP,Serial #devices Cuda...

echo "make -j kokkoskernels-install -f ${KOKKOSKERNELS_PATH}/Makefile.kokkos-kernels KOKKOS_PATH=${KOKKOS_PATH} KOKKOSKERNELS_SCALARS=${KOKKOSKERNELS_SCALARS} KOKKOSKERNELS_LAYOUTS=${KOKKOSKERNELS_LAYOUTS} KOKKOSKERNELS_ORDINALS=${KOKKOSKERNELS_ORDINALS} KOKKOSKERNELS_OFFSETS=${KOKKOSKERNELS_OFFSETS}  KOKKOSKERNELS_PATH=${KOKKOSKERNELS_PATH} CXX=${CXX} KOKKOSKERNELS_INSTALL_PATH=${KOKKOSKERNELS_INSTALL_PATH} KOKKOSKERNELS_OPTIONS=${KOKKOSKERNELS_OPTIONS} KOKKOS_DEVICES=${KOKKOS_DEVICES}"
make -j kokkoskernels-install -f ${KOKKOSKERNELS_PATH}/Makefile.kokkos-kernels KOKKOS_PATH=${KOKKOS_PATH} KOKKOSKERNELS_SCALARS=${KOKKOSKERNELS_SCALARS} KOKKOSKERNELS_LAYOUTS=${KOKKOSKERNELS_LAYOUTS} KOKKOSKERNELS_ORDINALS=${KOKKOSKERNELS_ORDINALS} KOKKOSKERNELS_OFFSETS=${KOKKOSKERNELS_OFFSETS}  KOKKOSKERNELS_PATH=${KOKKOSKERNELS_PATH} CXX=${CXX} KOKKOSKERNELS_INSTALL_PATH=${KOKKOSKERNELS_INSTALL_PATH} KOKKOSKERNELS_OPTIONS=${KOKKOSKERNELS_OPTIONS} KOKKOS_DEVICES=${KOKKOS_DEVICES}

