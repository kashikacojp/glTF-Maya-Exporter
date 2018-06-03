■ glTF-Maya-Exporter
　株式会社カシカ

★インストール方法
　以下のAとBの２種類の方法があります。

　A: MAYAのシステムフォルダにインストール

　　C:\Program Files\Autodesk\Maya2018\bin\plug-ins　　に　glTFExporter.mll 　　　　　　をコピー
　　C:\Program Files\Autodesk\Maya2018\scripts\others　に　glTFExporterOptions.mel　　　をコピー
　　C:\Program Files\Autodesk\Maya2018\scripts\others　に　glTFExporterOptions.res.mel　をコピー

　B: ユーザーフォルダにインストール

　　C:\Users\[ユーザー名]\Documents\maya\2018\ にplug-ins フォルダを作成
　　C:\Users\[ユーザー名]\Documents\maya\2018\plug-ins に glTFExporter.mll　 　　　　　をコピー
　　C:\Users\[ユーザー名]\Documents\maya\2018\ja_JP\scripts　に　glTFExporterOptions.mel　　　をコピー
　　C:\Users\[ユーザー名]\Documents\maya\2018\ja_JP\scripts　に　glTFExporterOptions.res.mel　をコピー
　　Mayaを起動し、putevn "MAYA_PLUG_IN_PATH" "C:\Users\[ユーザー名]\Documents\maya\2018\plug-ins" コマンドを実行する


　以上の設定を行い、ウインドウ→設定／プレファレンス→プラグインマネージャ　で、　自動ロード設定がONになっていると
　次回以降の起動時に自動的にプラグインが読み込まれます。


★使い方
　１．エクスポートしたいモデルを読み込みます。
　２．すべて書き出しを選択し、　"ファイルの種類" が glTFExporter になっていることを確認します。
　３．任意の場所に名前をつけて、すべて書き出しボタンを押します。
　　　（任意の場所に、指定した名前のフォルダが作成されます）

★オプションの意味
　エクスポート時に、ファイルタイプの特有のオプションが表示されます。

　- Recalc normals: 法線の再計算を行います。通常OFFで利用してください
　- Output glb file: gltf形式かglb形式を選択できます。
　- Compress buffer: GLTFのジオメトリデータ出力方法の指定。Bin: GLTFの標準バイナリ形式。Draco: GLTF拡張のDraco形式での出力。
　- Convert texture format: テクスチャデータの変換設定。N/A:変換しない。オリジナルファイルをコピー。 Jpeg: Jpegファイルに変換して出力。 Png: Pngファイルに変換して出力。

