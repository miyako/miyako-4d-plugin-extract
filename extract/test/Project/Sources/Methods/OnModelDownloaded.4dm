//%attributes = {"invisible":true,"preemptive":"capable"}
#DECLARE($params : Object; $context : Object)

var $folder : 4D:C1709.Folder
var $file : 4D:C1709.File

Case of 
	: (OB Instance of:C1731($context; cs:C1710.event.error))
		
		ALERT:C41($context.message)
		return 
		
	: (OB Instance of:C1731($context; cs:C1710.event.models))
		
		$file:=This:C1470.options.model  //llama-server
		
	Else 
		
		//this branch is executed in application process
		var $huggingface : cs:C1710.event.huggingface
		$huggingface:=$params.huggingfaces.huggingfaces.first()
		$folder:=$huggingface.folder
		If ($huggingface.name#"")  //ONNX
			$file:=$folder.file($huggingface.name)
		Else 
			$file:=$folder.file($huggingface.path)
		End if 
		
End case 

If (Not:C34($file.exists))
	ALERT:C41("model does not exist!")
	return 
End if 

If ($params.embeddings)
	Extract SET OPTION(Extract Option Tokenizer File; $file)
	ALERT:C41("Tokenizer loaded!")
End if 

If ($params.reranking)
	ALERT:C41("Reranker loaded!")
End if 