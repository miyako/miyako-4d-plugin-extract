![version](https://img.shields.io/badge/version-20%2B-E23089)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-get-localized-name)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-get-localized-name/total)

# 4d-plugin-get-localized-name

```4d
$folder:=Folder(fk desktop folder)
$name:=Get localized name($folder)
$file:=Folder(fk desktop folder).file("spreadsheet.xlsx")
$name:=Get localized name($file)
```

### macOS

```json
{
 "localizedName":"デスクトップ",
 "localizedTypeDescription":"フォルダ"
}

{
 "localizedName":"spreadsheet.xlsx",
 "localizedTypeDescription":"Microsoft Excel Workbook (.xlsx)",
 "localizedDescription":"Office Open XMLスプレッドシート"
}
```

### Windows

```json
{
 "localizedName":"デスクトップ",
 "localizedTypeDescription":"ファイル フォルダー"
}

{
 "localizedName":"spreadsheet.xlsx",
 "localizedTypeDescription":"XLSX ファイル"
 "localizedDescription":"XLSX ファイル"
}
```

