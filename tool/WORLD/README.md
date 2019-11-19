# About
WORLDのデモ用のアプリケーションです

## ディレクトリ構造
	/--doc		:ドキュメント(WORLD標準のもの)
	 |-test		:Windows Visual Studio用プロジェクトファイル群
	 |-Release	:Makeを走らせた際の標準的な出力先
	 	  |-f0	:Releaseディレクトリ内のテスト用run.shのf0出力先
	 	  |-sp	:sp出力先
	 	  |-ap	:ap出力先
	 	  |-conf:conf出力先
### ファイル群
ルートフォルダ内のファイルはtest.cpp以外’ほとんど’手を加えていない為，
APIの変更がない限りはtest.cppは使いまわせると思います．
簡単なテストがしてみたい場合は /Release/run.shを実行すれば分析合成を行います．

## 使い方
### オプション
	-i:	入力音声 (モノラルwav)
	-o:	出力合成音声
	-m:	分析及び合成のモード (ORを取った値を使用)
		0 -> 特徴量から音声を合成 (default)
		1 -> F0
		2 -> Spectrogram
		4 -> Aperiodicity
	-v:	バージョン表示
	--outf0: 出力f0ファイル名
	--outsp: 出力spファイル名
	--outap: 出力apファイル名
	--outconf: 出力分析設定ファイル名
	--f0floor: f0最低分析周波数 (default 71.0)
	--frameshift: フレームシフト量(0より大きい浮動小数) (default 5.0)
	--castfloat:	出力特徴量の型をfloatとして扱う

***
### ToDo
* Windows環境でのコマンドライン引数のパース
* Windows環境での分析設定の表示
  


