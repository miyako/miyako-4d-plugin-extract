//%attributes = {"invisible":true,"preemptive":"capable"}
TRUNCATE TABLE:C1051([Documents:1])
SET DATABASE PARAMETER:C642([Documents:1]; Table sequence number:K37:31; 0)
TRUNCATE TABLE:C1051([Embeddings:2])
SET DATABASE PARAMETER:C642([Embeddings:2]; Table sequence number:K37:31; 0)

var $AIClient : cs:C1710.AIKit.OpenAI
$AIClient:=cs:C1710.AIKit.OpenAI.new()
$AIClient.baseURL:="http://127.0.0.1:8080/v1"

var $file : 4D:C1709.File
var $extracted : Object

$files:=Folder:C1567("/RESOURCES/docx").files(fk ignore invisible:K87:22 | fk recursive:K87:7)\
.query("extension == :1"; ".docx")

var $e : cs:C1710.DocumentsEntity

For each ($file; $files)
	
	//for Harrier the tokens_length must be < max_position_embeddings
	$task:={file: $file; text_as_tokens: True:C214; \
		tokens_length: 1024; token_padding: True:C214; \
		overlap_ratio: 0.09; pooling_mode: Extract Pooling Mode Last}
	$extracted:=Extract(Extract Document DOCX; Extract Output Collection; $task)
	
	If ($extracted.success)
		$e:=ds:C1482.Documents.new()
		var $page : Object
		var $paragraphs; $documents : Collection
		$input:=$extracted.input
		$start:=Milliseconds:C459
		var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
		$batch:=$AIClient.embeddings.create($input)
		$duration:=(Milliseconds:C459-$start)/1000
		If ($batch.success)
			$e.duration:=(?00:00:00?)+$duration
			$e.prompt_tokens:=$batch.usage.prompt_tokens
			$e.save()
			var $ee : cs:C1710.EmbeddingsEntity
			For each ($embedding; $batch.embeddings)
				$ee:=ds:C1482.Embeddings.new()
				$ee.DocumentID:=$e.getKey()
				$ee.embedding:=$embedding.embedding
				//no text, tokens only
				//$ee.text:=$input.at($embedding.index)
				$ee.save()
			End for each 
		Else 
			$e.error:=$batch.errors.extract("body.error.message").join("\r")
			$e.save()
		End if 
	Else 
		TRACE:C157
	End if 
End for each 
