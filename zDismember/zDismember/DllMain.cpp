#include "UnionAfx.h"
#include <crtversion.h>
#include <../../_shared/ZSampler.h>

extern "C"
int __stdcall DllMain( HPLUGIN hModule, DWORD fdwReason, LPVOID lpvReserved )
{
  if( fdwReason == DLL_PROCESS_ATTACH )
  {
    Union.DefineCRTVersion( _VC_CRT_MAJOR_VERSION, _VC_CRT_MINOR_VERSION, _VC_CRT_BUILD_VERSION, _VC_CRT_RBUILD_VERSION );
  }
  if( fdwReason == DLL_PROCESS_DETACH )
  {
    // watek probkujacy: tylko flaga (czekanie na watek pod loader lockiem = zakleszczenie); konczony w Game_Exit
    zsamp::RequestStop();
  }
  return True;
}
