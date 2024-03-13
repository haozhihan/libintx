#include "libintx/cuda/md/engine.h"
#include "libintx/cuda/api/api.h"
#include "libintx/utility.h"
#include "test.h"
#include <iostream>

#include "libintx/reference.h"

using namespace libintx;
using namespace libintx::cuda;
using libintx::time;

const Double<3> rs[] = {
  {  0.7, -1.2, -0.1 },
  { -1.0,  0.0,  0.3 },
  { -1.0,  3.0,  1.3 },
  {  4.0,  1.0, -0.7 }
};

auto run(
  int A, int B, int C, int D,
  std::vector<Index2> Ks,
  int Nij, int Nkl)
{

  Basis<Gaussian> basis;

  std::array<size_t,2> dims{ Nij*npure(A)*npure(B), npure(C)*npure(D)*Nkl };
  auto buffer = device::vector<double>(dims[0]*dims[1]);

  printf("# (%i%i|%i%i) ", A, B, C, D);
  printf("dims: %ix%i, memory=%f GB\n", Nij, Nkl, 8*buffer.size()/1e9);

  struct {
    std::unique_ptr< libintx::IntegralEngine<4> > engine;
    double time = 0;
    std::vector<double> ratio;
  } md;

  for (auto K : Ks) {

    printf("# K={%i,%i}: ", K.first, K.second);

    auto [bra,ijs] = test::basis2({A,B}, {K.first,1}, Nij);
    auto [ket,kls] = test::basis2({C,D}, {K.second,1}, Nkl);

    cudaStream_t stream = 0;
    md.engine = libintx::cuda::md::eri<4>(bra, ket, stream);
    md.engine->compute(ijs, kls, buffer.data(), dims);
    libintx::cuda::stream::synchronize(stream);
    {
      auto t0 = time::now();
      md.engine->compute(ijs, kls, buffer.data(), dims);
      libintx::cuda::stream::synchronize(stream);
      double t = time::since(t0);
      md.time = 1/t;
    }

    printf("T(MD)=%f ", 1/md.time);
    printf("Int/s=%4.2e ", (Nij*Nkl)*md.time);
    double tref = reference::time(Nij*Nkl, bra[0], bra[1], ket[0], ket[1]);
    printf("T(Ref)=%f ", tref);
    printf("T(Ref/MD)=%f ", tref*md.time);
    printf("\n");

  } // Ks

}

#define RUN(A,B,C,D,...)             \
  if (test::enabled(A,B,C,D)) run(A,B,C,D,__VA_ARGS__);

int main(int argc, char **argv) {

  int load = 128;
  if (argc > 1) load = std::atoi(argv[1]);

  std::vector<Index2> Ks = {
    {1,1}, {1,5}, {5,5}
  };

  auto N = [&](int L) {
    int p = 1;
    for (int l = 1; l <= L; ++l) {
      p *= (l%2 ? 1 : 2);
    }
    return (8*load)/p;
  };

  // for (int a = 0; a <= LMAX; ++a) {
  //   for (int b = 0; b <= LMAX; ++b) {
  //     RUN(a,b,a,b, Ks, N(a), N(b));
  //   }
  // }

  std::vector<int> loadv = { load*32, load*16, load*8, load*4, load*2, load*1, load*1 };

  // (x,x,x,x)
  for (int l = 0; l <= LMAX; ++l) {
    RUN(l,l,l,l, Ks, loadv.at(l), loadv.at(l));
  }

  // (x,x,s,s)
  for (int l = 1; l <= LMAX; ++l) {
    RUN(0,0,l,l, Ks, 2*loadv.at(0), loadv.at(l));
  }

  // (x,s,x,s)
  for (int l = 1; l <= LMAX; ++l) {
    RUN(l,0,l,0, Ks, 2*loadv.at(l), 2*loadv.at(l));
  }

}
