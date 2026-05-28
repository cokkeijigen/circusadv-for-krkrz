#include <windows.h>
#include "ncbind.hpp"


#define EXPORT(hr) extern "C" __declspec(dllexport) hr __stdcall

#ifdef _MSC_VER
#pragma comment(linker, "/EXPORT:V2Link=_V2Link@4")
#pragma comment(linker, "/EXPORT:V2Unlink=_V2Unlink@0")
#endif

//--------------------------------------
int WINAPI
DllEntryPoint(HINSTANCE /*hinst*/, unsigned long /*reason*/, void* /*lpReserved*/)
{
	return 1;
}

//---------------------------------------------------------------------------
static tjs_int GlobalRefCountAtInit = 0;

EXPORT(HRESULT) V2Link(iTVPFunctionExporter *exporter)
{
	// スタブの初期化(必ず记述する)
	TVPInitImportStub(exporter);

	NCB_LOG_W("V2Link");

	// AutoRegisterで登録されたクラス等を登録する
	ncbAutoRegister::AllRegist();

	// この时点での TVPPluginGlobalRefCount の値を
	GlobalRefCountAtInit = TVPPluginGlobalRefCount;
	// として控えておく。TVPPluginGlobalRefCount はこのプラグイン内で
	// 管理されている tTJSDispatch 派生オブジェクトの参照カウンタの総计で、
	// 解放时にはこれと同じか、これよりも少なくなってないとならない。
	// そうなってなければ、どこか别のところで関数などが参照されていて、
	// プラグインは解放できないと言うことになる。

	return S_OK;
}
//---------------------------------------------------------------------------
EXPORT(HRESULT) V2Unlink()
{
	// 吉里吉里侧から、プラグインを解放しようとするときに呼ばれる関数

	// もし何らかの条件でプラグインを解放できない场合は
	// この时点で E_FAIL を返すようにする。
	// ここでは、TVPPluginGlobalRefCount が GlobalRefCountAtInit よりも
	// 大きくなっていれば失败ということにする。
	if (TVPPluginGlobalRefCount > GlobalRefCountAtInit) {
		NCB_LOG_W("V2Unlink ...failed");
		return E_FAIL;
		// E_FAIL が帰ると、Plugins.unlink メソッドは伪を返す
	} else {
		NCB_LOG_W("V2Unlink");
	}
	/*
		ただし、クラスの场合、厳密に「オブジェクトが使用中である」ということを
		知るすべがありません。基本的には、Plugins.unlink によるプラグインの解放は
		危険であると考えてください (いったん Plugins.link でリンクしたら、最后ま
		でプラグインを解放せず、プログラム终了と同时に自动的に解放させるのが吉)。
	*/

	// AutoRegisterで登録されたクラス等を削除する
	ncbAutoRegister::AllUnregist();

	// スタブの使用终了(必ず记述する)
	TVPUninitImportStub();

	return S_OK;
}


//---------------------------------------------------------------------------
// static変数の実体

// auto register 先头ポインタ
ncbAutoRegister::ThisClassT const*
ncbAutoRegister::_top[ncbAutoRegister::LINE_COUNT] = NCB_INNER_AUTOREGISTER_LINES_INSTANCE;

