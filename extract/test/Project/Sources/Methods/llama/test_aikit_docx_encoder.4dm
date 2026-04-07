//%attributes = {"invisible":true}
var $file : 4D:C1709.File
var $extracted : Object

$file:=File:C1566("/RESOURCES/sample.docx")

$task:={\
file: $file; \
unique_values_only: True:C214; \
max_paragraph_length: 10}

/*
max_paragraph_length: n
each [] will contain up to n lines
*/

$start_extraction:=Milliseconds:C459
$extracted:=Extract(Extract Document DOCX; Extract Output Collection; $task)
$duration_extraction:=Abs:C99(Milliseconds:C459-$start_extraction)

If ($extracted.success)
	var $AIClient : cs:C1710.AIKit.OpenAI
	$AIClient:=cs:C1710.AIKit.OpenAI.new()
	$AIClient.baseURL:="http://127.0.0.1:8080/v1"
/*
embeddings
expect collection of text
*/
	var $results : Collection
	$results:=[]
	var $paragraphs; $input : Collection
	var $len; $pos : Integer
	$start_embeddings:=Milliseconds:C459
	$paragraphs:=$extracted.input
	$len:=4  //number of paragraphs to process 
	$pos:=0  //slicing offset
	$input:=$paragraphs.slice($pos; $pos+$len)
	var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
	While ($input.length#0)
		$batch:=$AIClient.embeddings.create($input)
		If ($batch.success)
			var $sentences : Collection
			$sentences:=$extracted.input
			var $embedding : Object
			For each ($embedding; $batch.embeddings)
				$vector:=$embedding.embedding
				$index:=$embedding.index  //sentence id in batch
				$text:=$sentences.at($index)
				$results.push({vector: $vector; text: $text})
			End for each 
		End if 
		$pos+=$len
		$input:=$paragraphs.slice($pos; $pos+$len)
	End while 
	$duration_embeddings:=Abs:C99(Milliseconds:C459-$start_embeddings)
End if 

/*

granite onnx
{
"time": "6.659 seconds total",
"count": 84,
"average": "12.614 embeddings per second"
}

granite llama
{
"time": "3.886 seconds total",
"count": 84,
"average": "21.616 embeddings per second"
},
{
"time": "6.107 seconds total",
"count": 835,
"average": "136.728 embeddings per second"
}

harrier llama
{
"time": "23.213 seconds total",
"count": 84,
"average": "3.618 embeddings per second"
},
{
"time": "27.415 seconds total",
"count": 835,
"average": "30.457 embeddings per second"
}

*/

ALERT:C41(JSON Stringify:C1217({\
time: String:C10(($duration_extraction+$duration_embeddings)/1000)+" seconds total"; \
count: $results.length; \
average: String:C10($results.length/(($duration_extraction+$duration_embeddings)/1000); "####0.000")+" embeddings per second"}; *))