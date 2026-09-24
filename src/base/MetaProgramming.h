//////////////////////////////////////////////////////////////////////////////////
//�ļ���MetaProgramming.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-03
//������Ԫ����
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_MetaProgramming_H_
#define __EtherDB_MetaProgramming_H_

namespace EtherDB
{
	template <typename T>
	struct IsReference
	{
		enum
		{
			VALUE = 0
		};
	};

	template <typename T>
	struct IsReference<T&>
	{
		enum
		{
			VALUE = 1
		};
	};

	template <typename T>
	struct IsReference<const T&>
	{
		enum
		{
			VALUE = 1
		};
	};

	template <typename T>
	struct IsConst
	{
		enum
		{
			VALUE = 0
		};
	};

	template <typename T>
	struct IsConst<const T&>
	{
		enum
		{
			VALUE = 1
		};
	};


	template <typename T>
	struct IsConst<const T>
	{
		enum
		{
			VALUE = 1
		};
	};

	template <typename T, int i>
	struct IsConst<const T[i]>
	{
		enum
		{
			VALUE = 1
		};
	};

	template <typename T>
	struct TypeWrapper
	{
		typedef T TYPE;
		typedef const T CONSTTYPE;
		typedef T& REFTYPE;
		typedef const T& CONSTREFTYPE;
	};

	template <typename T>
	struct TypeWrapper<const T>
	{
		typedef T TYPE;
		typedef const T CONSTTYPE;
		typedef T& REFTYPE;
		typedef const T& CONSTREFTYPE;
	};


	template <typename T>
	struct TypeWrapper<const T&>
	{
		typedef T TYPE;
		typedef const T CONSTTYPE;
		typedef T& REFTYPE;
		typedef const T& CONSTREFTYPE;
	};


	template <typename T>
	struct TypeWrapper<T&>
	{
		typedef T TYPE;
		typedef const T CONSTTYPE;
		typedef T& REFTYPE;
		typedef const T& CONSTREFTYPE;
	};
}


#endif // !__EtherDB_MetaProgramming_H_
