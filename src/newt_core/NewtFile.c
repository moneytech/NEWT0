/*------------------------------------------------------------------------*/
/**
 * @file	NewtFile.c
 * @brief   ファイル処理
 *
 * @author  M.Nukui
 * @date	2004-01-25
 *
 * Copyright (C) 2003-2004 M.Nukui All rights reserved.
 */


/* ヘッダファイル */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __WIN32__
	#include "win/dlfcn.h"
#else
	#include <dlfcn.h>
	#include <pwd.h>
#endif

#include "NewtCore.h"
#include "NewtVM.h"
#include "NewtBC.h"
#include "NewtIO.h"
#include "NewtFile.h"

/*------------------------------------------------------------------------*/
/** 動的ライブラリをインストールする
 *
 * @param fname		[in] ファイルのパス
 *
 * @return			動的ライブラリのデスクプリタ
 */

void * NewtDylibInstall(const char * fname)
{
    newt_install_t	install_call;
    void *	lib;

    lib = dlopen(fname, RTLD_LAZY);

    if (lib != NULL)
    {
        install_call = (newt_install_t)dlsym(lib, "newt_install");

        if (install_call == NULL)
        {
            dlclose(lib);
            return NULL;
        }

        (*install_call)();
    }

    return lib;
}


/*------------------------------------------------------------------------*/
/** ファイルの存在確認
 *
 * @param path		[in] ファイルのパス
 *
 * @retval			true	ファイルが存在する
 * @retval			false   ファイルが存在しない
 */

bool NewtFileExists(char * path)
{
	FILE *  f;

	f = fopen(path, "r");

	if (f != NULL)
	{
		fclose(f);
		return true;
	}

	return false;
}


#pragma mark -
/*------------------------------------------------------------------------*/
/** ホームディレクトリのパスを取得
 *
 * @param s			[in] ファイルのパス
 * @param subdir	[out]サブディレクトリ
 *
 * @return			ホームディレクトリのパス
 *
 * @note			取得されたホームディレクトリの文字列は free する必要がある
 */

#ifdef __WIN32__

char * NewtGetHomeDir(const char * s, char ** subdir)
{	// Windows の場合
	return NULL;
}

#else

char * NewtGetHomeDir(const char * s, char ** subdir)
{	// UNIX の場合
	struct passwd * pswd = NULL;
	uint32_t	len;
	char *  login = NULL;
	char *  dir = NULL;
	char *  sep;

	sep = strchr(s + 1, '/');

	if (sep != NULL)
	{
		len = sep - (s + 1);
		login = malloc(len + 1);
		strncpy(login, s + 1, len);
		pswd = getpwnam(s + 1);
	}
	else
	{
		login = (char *)s + 1;
	}

	if (*login != '\0')
		pswd = getpwnam(login);
	else
		pswd = getpwuid(getuid());

	if (pswd != NULL)
		dir = pswd->pw_dir;

	if (subdir != NULL)
		*subdir = sep;

	if (s + 1 != login)
		free(login);

	return dir;
}

#endif


/*------------------------------------------------------------------------*/
/** ディレクトリ名とファイル名からパスを作成
 *
 * @param s1		[in] ディレクトリ名
 * @param s2		[in] ファイル名
 *
 * @return			作成されたパス
 *
 * @note			取得されたホームディレクトリの文字列は free する必要がある
 */

char * NewtJoinPath(char * s1, char * s2)
{
	char *		path;
	uint32_t	len;
	uint32_t	len1;
	uint32_t	len2;

	len1 = strlen(s1);
	len2 = strlen(s2);

	len = len1 + len2 + 2;

	path = malloc(len);
	if (path == NULL) return NULL;

	strcpy(path, s1);

	path[len1] = '/';
	strncpy(path + len1 + 1, s2, len2 + 1);

	return path;
}


/*------------------------------------------------------------------------*/
/** 相対パスを絶対パスに展開する
 *
 * @param s			[i/o]相対パス→絶対パス
 *
 * @return			絶対パス
 */

char * NewtRelToAbsPath(char * s)
{
	char *  src;
	char *  dst;

	for (src = dst = s; *src != '\0';)
	{
		if (src[0] == '/' && src[1] == '.')
		{
			if (src[2] == '/' || src[2] == '\0')
			{
				src += 2;
				continue;
			}
			else if (src[2] == '.' && src[3] == '/')
			{
				src += 3;

				while (s < dst)
				{
					dst--;
					if (*dst == '/') break;
				}

				continue;
			}
		}

		if (src != dst)
			*dst = *src;

		src++;
		dst++;
	}

	if (s < dst && *(dst - 1) == '/')
		*(dst - 1) = '\0';
	else if (src != dst)
		*dst = '\0';

	return s;
}


/*------------------------------------------------------------------------*/
/** 相対パスを絶対パスに展開する
 *
 * @param s			[in] 相対パス（C文字列）
 *
 * @return			絶対パス（文字列オブジェクト）
 *
 * @note			~, ~user はホームディレクトリに展開される
 */

newtRef NewtExpandPath(const char *	s)
{
    newtRefVar	r = kNewtRefUnbind;
	char *  subdir = NULL;
	char *  dir = NULL;
	char *  wd = NULL;

	switch (*s)
	{
		case '/':
			dir = (char *)s;
			break;

		case '~':
			dir = NewtGetHomeDir(s, &subdir);

			if (subdir != NULL && subdir[1] != '\0')
				subdir++;
			else
				subdir = NULL;
			break;

		default:
			subdir = (char *)s;
			break;
	}

	if (dir == NULL)
		dir = wd = getcwd(NULL, 0); // for Mac OS X 10.2
//		dir = wd = getwd(NULL);

	if (subdir != NULL)
	{
		dir = NewtJoinPath(dir, subdir);
		NewtRelToAbsPath(dir);
	}

	r = NSSTR(dir);

	if (subdir != NULL)
		free(dir);

	if (wd != NULL)
		free(wd);

	return r;
}


#pragma mark -
/*------------------------------------------------------------------------*/
/** パスからファイル名を取出す
 *
 * @param s			[in] パスへのポインタ
 * @param len		[in] パスの文字数
 *
 * @return			ファイル名
 */

char * NewtBaseName(char * s, uint32_t len)
{
	uint32_t	base = 0;
	uint32_t	i;

	for (i = 0; i < len; i++)
	{
		if (s[i] == '/')
			base = i + 1;
	}

	if (base < len)
		return (s + base);
	else
		return NULL;
}


#pragma mark -
/*------------------------------------------------------------------------*/
/** ライブラリのロード
 *
 * @param rcvr		[in] レシーバ
 * @param r			[in] ロードするライブラリのパス
 *
 * @return			動的ライブラリのデスクプリタ
 */

newtRef NsLoadLib(newtRefArg rcvr, newtRefArg r)
{
    char *	fname;
    void *	lib;

    if (! NewtRefIsString(r))
        return NewtThrow(kNErrNotAString, r);

    fname = NewtRefToString(r);
    lib = NewtDylibInstall(fname);

    if (lib != NULL)
	{
        return NewtMakeInteger((int32_t)lib);
	}
	else
	{
		const char *  errmsg;

		errmsg = dlerror();

		if (errmsg != NULL)
		{
			NewtFprintf(stderr,"%s\n", errmsg);
		}

        return NewtThrow(kNErrDylibNotOpen, r);
	}
}


/*------------------------------------------------------------------------*/
/** ソースファイルのロード
 *
 * @param rcvr		[in] レシーバ
 * @param r			[in] ロードするソースファイルのパス
 *
 * @return			実行結果のオブジェクト
 */

newtRef NsLoad(newtRefArg rcvr, newtRefArg r)
{
	newtRefVar  result = kNewtRefUnbind;
    newtRefVar	fn;
    char *	fname;

    if (! NewtRefIsString(r))
        return NewtThrow(kNErrNotAString, r);

    fname = NewtRefToString(r);
    fn = NBCCompileFile(fname, true);

    if (NewtRefIsNotNIL(fn))
        result = NVMCall(fn, 0, NULL);

    return result;
}


/*------------------------------------------------------------------------*/
/** ライブラリの要求
 *
 * @param r			[in] ロードするライブラリのシンボル文字列
 *
 * @return			ロードされたライブラリのシンボル
 *
 * @note			シンボルによりライブラリを要求する。
 *					拡張子は必要ない。適宜ライブラリパスにより検索される。
 *					一度ロードされたライブラリは読込まれない。
 *					ライブラリが見つからなくても例外は発生しない。
 */

newtRef NcRequire0(newtRefArg r)
{
	newtRefVar  newtlib;
	newtRefVar  requires;
	newtRefVar  sym;
	newtRefVar  env;

	if (NewtRefIsSymbol(r))
	{
		sym = r;
	}
	else
	{
		if (! NewtRefIsString(r))
			return NewtThrow(kNErrNotASymbol, r);

		sym = NcMakeSymbol(r);
	}

    requires = NcGetGlobalVar(NSSYM0(requires));

	if (! NewtRefIsFrame(requires))
	{
		requires = NcMakeFrame();
		NcDefGlobalVar(NSSYM0(requires), requires);
	}

	if (NewtHasSlot(requires, sym))
		return kNewtRefNIL;

	{
		newtRefVar	initObj[] = {kNewtRefUnbind, NSSTR("."), kNewtRefUnbind};
		struct {
			newtRefVar  exts;
			bool		dylib;
		} lib_exts[] = {
			{NSSTR("dylib"),	true},
			{NSSTR("so"),		true},
			{NSSTR("dll"),		true},
			{NSSTR("newt"),		false},
		};

		newtRefVar  lib;
		newtRefVar  dir;
		newtRefVar  patharray;
		newtRefVar  path;
		uint32_t	len;
		uint32_t	i;
		uint32_t	j;

		patharray = NewtMakeArray2(kNewtRefNIL, sizeof(initObj) / sizeof(newtRefVar), initObj);

		env = NcGetGlobalVar(NSSYM0(_ENV_));
		newtlib = NcGetVariable(env, NSSYM0(NEWTLIB));

		if (NewtRefIsNIL(newtlib))
		{
			newtRefVar	initPath[] = {NSSTR("."), NcGetGlobalVar(NSSYM0(_EXEDIR_))};

			newtlib = NewtMakeArray2(kNewtRefNIL, sizeof(initPath) / sizeof(newtRefVar), initPath);
		}

		len = NewtLength(newtlib);

		for (i = 0; i < len; i++)
		{
			dir = NewtGetArraySlot(newtlib, i);
			NewtSetArraySlot(patharray, 0, NcJoinPath(dir, r));

			for (j = 0; j < sizeof(lib_exts) / sizeof(lib_exts[0]); j++)
			{
				NewtSetArraySlot(patharray, 2, lib_exts[j].exts);
				path = NcStringer(patharray);

				if (NewtFileExists(NewtRefToString(path)))
				{
					if (lib_exts[j].dylib)
					{
						lib = NcLoadLib(path);
						NcSetSlot(requires, sym, lib);
					}
					else
					{
						NcSetSlot(requires, sym, path);
						NcLoad(path);
					}

					return sym;
				}
			}
		}
	}

    return kNewtRefUnbind;
}


/*------------------------------------------------------------------------*/
/** ライブラリの要求
 *
 * @param rcvr		[in] レシーバ
 * @param r			[in] ロードするライブラリのシンボル文字列
 *
 * @return			ロードされたライブラリのシンボル
 *
 * @note			シンボルによりライブラリを要求する。
 *					拡張子は必要ない。適宜ライブラリパスにより検索される。
 *					一度ロードされたライブラリは読込まれない。
 */

newtRef NsRequire(newtRefArg rcvr, newtRefArg r)
{
	newtRefVar  result;

	result = NcRequire0(r);

	if (result == kNewtRefUnbind)
		NewtThrow(kNErrFileNotFound, r);

	return result;
}


#pragma mark -
/*------------------------------------------------------------------------*/
/** ファイルの存在確認
 *
 * @param rcvr		[in] レシーバ
 * @param r			[in] ファイルのパス
 *
 * @retval			true	ファイルが存在する
 * @retval			false   ファイルが存在しない
 *
 * @note			スクリプトからの呼出し用
 */

newtRef NsFileExists(newtRefArg rcvr, newtRefArg r)
{
    if (! NewtRefIsString(r))
        return NewtThrow(kNErrNotAString, r);

	return NewtMakeBoolean(NewtFileExists(NewtRefToString(r)));
}


#pragma mark -
/*------------------------------------------------------------------------*/
/** パスからディレクトリ名を取出す
 *
 * @param rcvr		[in] レシーバ
 * @param r			[in] ファイルのパス
 *
 * @return			ディレクトリ名
 */

newtRef NsDirName(newtRefArg rcvr, newtRefArg r)
{
	char *  base;
	char *  s;

    if (! NewtRefIsString(r))
        return NewtThrow(kNErrNotAString, r);

	s = NewtRefToString(r);
    base = NewtBaseName(s, NewtStringLength(r));

	if (base != NULL && s < base)
	{
		if (base - 1 != s && *(base - 1) == '/')
			base--;

		if (s < base)
			return NewtMakeString2(s, base - s, false);
	}

	return NSSTR(".");
}


/*------------------------------------------------------------------------*/
/** パスからファイル名を取出す
 *
 * @param rcvr		[in] レシーバ
 * @param r			[in] ファイルのパス
 *
 * @return			ファイル名
 *
 * @note			スクリプトからの呼出し用
 */

newtRef NsBaseName(newtRefArg rcvr, newtRefArg r)
{
	char *  base;

    if (! NewtRefIsString(r))
        return NewtThrow(kNErrNotAString, r);

    base = NewtBaseName(NewtRefToString(r), NewtStringLength(r));

	if (base != NULL)
		return NSSTR(base);
	else
		return r;
}


/*------------------------------------------------------------------------*/
/** ディレクトリ名とファイル名からパスを作成
 *
 * @param rcvr		[in] レシーバ
 * @param r1		[in] ディレクトリ名
 * @param r2		[in] ファイル名
 *
 * @return			作成されたパス
 */

newtRef NsJoinPath(newtRefArg rcvr, newtRefArg r1, newtRefArg r2)
{
	newtRefVar	initObj[] = {r1, NSSTR("/"), r2};
	newtRefVar  r;

	r = NewtMakeArray2(kNewtRefNIL, sizeof(initObj) / sizeof(newtRefVar), initObj);

	return NcStringer(r);
}


/*------------------------------------------------------------------------*/
/** 相対パスを絶対パスに展開する
 *
 * @param rcvr		[in] レシーバ
 * @param r			[in] 相対パス
 *
 * @return			絶対パス
 *
 * @note			~, ~user はホームディレクトリに展開される
 *					スクリプトからの呼出し用
 */

newtRef NsExpandPath(newtRefArg rcvr, newtRefArg r)
{
    if (! NewtRefIsString(r))
        return NewtThrow(kNErrNotAString, r);

    return NewtExpandPath(NewtRefToString(r));
}
