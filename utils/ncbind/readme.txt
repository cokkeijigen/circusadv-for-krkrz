Title: 吉里吉里プラグイン向けネイティブクラスバインダ
Author: miahmie


●これは何か？

既存の C++ ライブラリを TJS で扱うためのプラグインを
简単に作ることができるようにするための C++ テンプレートです。

従来のプラグインソースでは引数の受け渡し部分のラッパを
tp_stub.h のマクロを使いつつ手で书かなければならなかったのですが，
テンプレートによりこれをほとんどコンパイラに任せることができます。

gcc 3.4 系列と VC++2005 Express で动作确认しています。
テンプレート実装の関係で VC++6 ではコンパイルすら无理だと思います。
VC++2003 や他のコンパイラは未确认なのでどうなるかわかりません。


●简単な説明

マクロで登録したいクラスの定义を记述するだけです。

#include "ncbind.hpp"
#include "既存ライブラリのヘッダ等"

NCB_REGISTER_CLASS(登録するクラス名) {
  Constructor<コンストラクタの引数の型, ...>(0);
  Method("登録するメソッド名1", &Class::登録するメソッド1);
  Method("登録するメソッド名2", &Class::登録するメソッド2);
     :
  Method("登録するメソッド名n", &Class::登録するメソッドn);
}

详しくは testbind.cpp のソースを见るなどしてください。


●ファイル构成

  ncbind.hpp		メインテンプレート
  ncbind.cpp		V2Link/V2Unlink 定义
  ncbind.def		gcc用エクスポートファイル
			VC++では /EXPORT:V2Link /EXPORT:V2Unlink してください
  ncb_invoke.hpp	任意のメソッドを呼ぶためのテンプレート
  ncb_foreach.h		マクロを任意个数展开するためのincludeマクロ

  testbind.*		テスト用のプロジェクト


●详细


　▼NCB_REGISTER_CLASS(Class) { ... }
　▼NCB_REGISTER_CLASS_DIFFER(Name, Class) { ... }

Class を TJSグローバル空间(Name or Class)に登録します。
NCB_REGISTER_CLASS では Class が，NCB_REGISTER_CLASS_DIFFERでは Name が
グローバル空间上での名前となります。同じクラスの多重登録はエラーになります。


※ 以前のバージョンの NCB_{ CONSTRUCTOR, METHOD*, PROPERTY* } マクロも
　使用できますが，Bridge用のマクロは用意されていません。

----------------------------------------------------------------
　　▽Constructor();
　　▽Constructor<arg1, arg2, ...>(0);

引数の型のリスト (arg1, arg2, ...) を渡してコンストラクタを登録します。
直后の (0) は Constructor のオーバーロード用のシグネチャとして必要です。
デフォルトコンストラクタ（引数なし）を登録する场合はその限りではありません。

型の一致したコンストラクタが TJS から new するときに呼ばれます。
登録しない场合はそのクラスを new するとエラーになります。（※まだ未実装）
また，オーバーロードされた複数コンストラクタの登録はできません。
最初に登録されたものが有効となります。（実行时に警告メッセージが出ます）


----------------------------------------------------------------
　　▽Factory(Class* (*)(iTJSDispatch2*[, arg1, arg2, ...]));
　　▽Factory(tjs_error (TJS_INTF_METHOD *)(Class**, tjs_int, 
                                            tTJSVariant**, iTJSDispatch2*));

インスタンスを生成する関数を指定してコンストラクタを登録します。
１番目の引数にobjthisが渡るProxyのような関数を使用する方法と，引数の个数と
tTJSVariantの配列を渡すRawCallbackのような関数を使用する方法があります。

前者は返り値として生成したインスタンスのポインタを返します。
（NULLを返すとエラーとして扱われます）

后者は一番目の引数にインスタンスのポインタを书き込んでTJS_S_OKを返します。
（NULLを书き込んだり，TJS_S_OK以外を返すとエラーとして扱われます）


----------------------------------------------------------------
　　▽Method(Name, &Class::Method);

クラスメソッド Method を登録します。Class は NCB_REGISTER_CLASS にて
渡されたクラスが typedef されています。
Name は TJS_W() 経由のワイド文字でも，直记述のナロー文字列でもかまいません。
（ナローの场合は登録时に自动的に変换がかかります）

オーバーロード等で同じ名前のメソッドが複数ある场合は
メソッド型の自动判定に失败するので static_cast または method_cast を
使用してください。

　　static_cast<ReturnType (Class::*)(arg1, ... argN) [const]>(&Class::Method)
　　method_cast<ReturnType, Type, arg1, ... argN>(&Class::Method)

ReturnType は メソッドの返り値の型です。
Type はクラスメソッドのタイプで，
	Class	普通のクラスメソッド
	Const	const なクラスメソッド（ReturnType Method(arg...) const;）
	Static	static なクラスメソッド
の 3つのうちどれかを记述します。
arg1, ... argN は引数の型を指定します。



----------------------------------------------------------------
　　▽Method(Name, &Method, Proxy);

クラス外のstatic関数をクラスメソッドとして振舞わせるよう登録します。
その関数の一番目の引数は必ずそのクラスのインスタンスポインタ型にします。
これに this 相当の値が渡り，TJSから渡される残りの引数はその后に并べます。

既存のライブラリを登録するときに，ライブラリに手を加えず
何らかの特殊な処理を入れたいなどの场合で有効です。


----------------------------------------------------------------
　　▽Method(Name, &Method, Bridge<BridgeFunctor>());

ブリッジファンクタを指定して，そのクラス内から别のクラスインスタンスに
処理を委譲するメソッドを登録します。
ファンクタは以下のような形式です。

struct BridgeFunctor { 
  BridgeClass* operator()(Class* p) const {
    return p->BridgeInstance;
  }
};
	Class          : 登録中のクラス
	BridgeClass    : 処理を委譲するクラス
	BridgeInstance : Class内に保持されている BridgeClass のインスタンス

Methodは委譲先のクラスのメソッドを记述します。
&Class::Method ではなく，&BridgeClass::Method という表记になります。


----------------------------------------------------------------
　　▽Method(Name, &Method, ProxyBridge<BridgeFunctor>());
　　▽Method(Name, &Method, BridgeProxy<BridgeFunctor>());

Proxyつきのブリッジ登録です。


----------------------------------------------------------------
　　▽Property(Property, GetterMethod, SetterMethod);

Property という名前でプロパティを登録します。
GetterMethod, SetterMethod はそれぞれ
プロパティ取得，プロパティ设定メソッドです。
SetterMethod または GetterMethod に 0 を渡すと，
それぞれ は読み込み専用，书き込み専用プロパティになります。

Setter/Getterのメソッド型のチェックが甘いので
指定するメソッドには注意してください。

----------------------------------------------------------------
　　▽Property(Property, GetterMethod, SetterMethod, Proxy);
　　▽Property(Property, GetterMethod, SetterMethod, Bridge<Functor>());
　　▽Property(Property, GetterMethod, SetterMethod, ProxyBridge<Functor>());
　　▽Property(Property, GetterMethod, SetterMethod, BridgeProxy<Functor>());

Proxy/Bridgeつきのプロパティを登録します。
详细は Method 项目を参照してください。



----------------------------------------------------------------
　　▽RawCallback(Name, &Callback, Flag);

コールバックを指定して Name という名前でメソッドを登録します。
Callback は tTJSNativeClassMethodCallback 型の static 関数ポインタか，
または，tTJSNativeClassMethodCallback の引数の iTJSDispatch2 *objthis を
このクラスのインスタンスのポインタにしたものが使えます。
（この场合，実际のネイティブインスタンスへのポインタが引数として渡ります）

Flag は，
	0		 (通常クラスメソッド时)
	TJS_STATICMEMBER (staticメソッド时) 
のどちらかが指定できます。

----------------------------------------------------------------
　　▽RawCallback(Name, &GetterCallback, &SetterCallback, Flag);

コールバック指定のプロパティ登録です。



----------------------------------------------------------------

マクロ説明の続き：


　▼NCB_ATTACH_CLASS(          Class, TJS2Class) { ... }
　▼NCB_ATTACH_CLASS_WITH_HOOK(Class, TJS2Class) { ... }

吉里吉里２の既存のクラス TJS2Class に Class を付加します。
{ ... } スコープ内は NCB_REGISTER_CLASS と同じように定义します。
ただし，コンストラクタ定义 NCB_CONSTRUCTOR は使えません。

NCB_ATTACH_CLASS で登録した场合，Class のインスタンスは
登録された Class のメソッドが初めて呼ばれたときに
引数なしのコンストラクタで new され，メソッドが呼ばれます。

NCB_ATTACH_CLASS_WITH_HOOK で登録する场合は，あらかじめ后述の
NCB_GET_INSTANCE_HOOK が定义済みでなければなりません。
Class インスタンスの生成はフック用クラスに一任されます。


　▼NCB_GET_INSTANCE_HOOK(Class) { ... };
　　▼NCB_GET_INSTANCE_HOOK_CLASS
　　▼NCB_INSTANCE_GETTER(ObjThis)

TJS からネイティブクラス Class のメソッドを呼び出す时の
インスタンスを取得する関数をフック（というか再定义）します。
详しくは testbind.cpp ソースを参照してください。

NCB_ATTACH_CLASS だけでなく，NCB_REGISTER_CLASS で
登録したクラスにも适用されます。

また，すべてのクラスメソッドに适用されるため，
指定の个别メソッドにフックする等はできません。
　⇒要望があれば実装します


　▼NCB_REGISTER_SUBCLASS(Class)
　　▽SubClass(Name, TypeWrap<Class>());

サブクラスの定义と登録です。クラス中にstaticとしてサブクラスが置かれます。
これを使うことで名前空间を汚さずに阶层构造をとることができます。
※実験中コードにつき不具合があるかもしれません


　▼NCB_REGISTER_FUNCTION(Name, Function);

TJS グローバル空间に Name という名前で Function という関数を登録します。
Function が tTJSNativeClassMethodCallback 型の场合は
そのまま RawCallback 形式のグローバル関数として动作します。


　▼NCB_ATTACH_FUNCTION(Name, TJS2Class, Function);

吉里吉里２の既存のクラス TJS2Class に Name という名前で
Function を付加します。Function が tTJSNativeClassMethodCallback 型の场合は
そのままメソッドの RawCallback と同じように动作します。

このマクロでは，非 static な関数しか付加できません。static なメソッドとして
登録したい场合は，ダミーの static メソッドしか存在しないクラスを作り，
NCB_ATTACH_CLASS で登録してください。（static メソッドの呼び出しであれば
ネイティブインスタンスは生成されません）



　▼NCB_REGISTER_INSTANCE(...); // 未実装

　　⇒ 参照を返す function / property で代用できます


　▼NCB_TYPECONV_CAST(Type, CastType);

引数の型が Type の场合，キャスト CastType を指定して
tTJSVariant と相互変换するように登録します。

　▼NCB_SET_CONVERTOR(Type, Convertor);

引数の型 Type を変换するクラスを登録します。
详しくは ncbind.hpp のコメント等を参照してください。



　▼NCB_PRE_REGIST_CALLBACK(Callback);
　▼NCB_POST_REGIST_CALLBACK(Callback);
　▼NCB_PRE_UNREGIST_CALLBACK(Callback);
　▼NCB_POST_UNREGIST_CALLBACK(Callback);

クラスを登録・开放する前后に呼ばれるコールバック void Callback() を
登録します。呼ばれる顺番は以下のとおりです。

V2Link时：
	PRE_REGIST_CALLBACK
	クラス登録
	POST_REGIST_CALLBACK
V2Unlink时：
	PRE_UNREGIST_CALLBACK
	クラス开放
	POST_UNREGIST_CALLBACK

同じ种类のコールバックが複数登録された场合の顺番の保証はありません。



●制限

・継承関係はサポートしていない
　⇒継承関係にあるクラス同士を登録した场合，别のクラス扱いになる
	・instanceof で派生クラスインスタンスのチェックができない
	・引数に派生クラスインスタンスを渡した场合は
	　インスタンスポインタが取得できずにエラーとなる

・引数の省略によるデフォルト値をサポートしていない
　⇒TJSから渡す引数の个数はメソッドの引数の个数以上であること
　　余分に渡された引数は无视され，足りない场合は TJS_E_BADPARAMCOUNT が返る
　⇒可変长引数をサポートする场合は RawCallback で自力実装すること

・namespace 内でのクラス登録は考えられていないので namespace 外で行うこと
　⇒namespace 内のクラスを登録する场合は typedef するなどで
　　必ず namespace の外で行う (:: を含むとエラーになる)

  ex.
	typedef ::Foo::Bar::TargetClass TargetClass;

	NCB_REGISTER_CLASS(TargetClass) { ... }


・コンストラクタは１つしか记述できない
　⇒インスタンスを生成する static なメソッドを书くなどして解决する

・登録したクラスのインスタンスをメソッドの返り値で返す时には注意が必要
　⇒インスタンスの多重 delete や，既に delete されている无効な
　　インスタンスが使用される可能性がある

コピーで返す　：コピーコンストラクタで新たなインスタンスが生成されTJS侧へ返る
参照で返す　　：そのままTJS侧へ返され，invalidate 时に delete されない
ポインタで返す：そのままTJS侧へ渡され，invalidate 时に delete される
const の参照/ポインタは强制的に const 解除される


・参照で値を书き换えて返すようなメソッドには対応できない
　⇒适当なRawCallback関数を书くなどして対処すること

・TJSCreateNativeClassMethod を使わずにメソッド呼び出しを独自実装
　しているため，将来にわたりソース互换が保てるという保証がない
　⇒吉里吉里２自体もうメンテフェーズだから别にいい…よね？


●独り言

・テンプレートは Modern C++ Design 书籍を読んだくらいで，Boostとかｼﾗﾈ
　ncb_invokeの前身バージョンでTypeListで実装したやつもあったけど
　そこまでするような物ではないと悟ったので今回は力业で実现

・tTJSVariantType tTJSVariant::Type() は何故 const メソッドでないのか
　const_cast使うハメになって败北気分
	⇒修正されましたが，従来互换重视のため结局使えない（涙）



●TODOメモ

・ncibind.hpp 古いコメントの整理
・NCB_SET_CONVERTOR テスト
・Attach时に既存のメソッドがあった场合の処理
・オーバーロードと引数の省略をサポートするか？
