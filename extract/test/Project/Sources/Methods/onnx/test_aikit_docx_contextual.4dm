//%attributes = {"invisible":true}
var $file : 4D:C1709.File
var $extracted : Object

$file:=File:C1566("/RESOURCES/sample.docx")

$task:={\
file: $file; \
unique_values_only: True:C214; \
max_paragraph_length: 100}

/*
max_paragraph_length: n
each [] will contain up to n paragraphs
*/

$start_extraction:=Milliseconds:C459
$extracted:=Extract(Extract Document DOCX; Extract Output Collections; $task)
$duration_extraction:=Abs:C99(Milliseconds:C459-$start_extraction)

If ($extracted.success)
	var $AIClient : cs:C1710.AIKit.OpenAI
	$AIClient:=cs:C1710.AIKit.OpenAI.new()
	$AIClient.baseURL:="http://127.0.0.1:8080/v1/contextualized"
/*
contextualizedembeddings
expect collection of collection of text
*/
	var $results : Collection
	$results:=[]
	var $paragraphs; $inputs : Collection
	var $len; $pos : Integer
	$start_embeddings:=Milliseconds:C459
	$paragraphs:=$extracted.inputs  //paragraphs in page
	$len:=1  //number of paragraphs to process 
	$pos:=0  //slicing offset
	$inputs:=$paragraphs.slice($pos; $pos+$len)
	var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
	While ($inputs.length#0)
		$batch:=$AIClient.embeddings.create($inputs)
		If ($batch.success)
			var $sentences : Collection
			$sentences:=$inputs.flat()
			var $embedding : Object
			For each ($embedding; $batch.embeddings)
				$vector:=$embedding.embedding
				$index:=$embedding.index  //sentence id in batch
				$text:=$sentences.at($index)
				$results.push({vector: $vector; text: $text})
			End for each 
		End if 
		$pos+=$len
		$inputs:=$paragraphs.slice($pos; $pos+$len)
	End while 
	$duration_embeddings:=Abs:C99(Milliseconds:C459-$start_embeddings)
End if 

/*
pplx
{
"time": "1136.722 seconds total",
"count": 835,
"average": "0.734 embeddings per second"
}
granite
{
"time": "43.698 seconds total",
"count": 835,
"average": "19.108 embeddings per second"
}
*/

ALERT:C41(JSON Stringify:C1217({\
time: String:C10(($duration_extraction+$duration_embeddings)/1000)+" seconds total"; \
count: $results.length; \
average: String:C10($results.length/(($duration_extraction+$duration_embeddings)/1000); "####0.000")+" embeddings per second"}; *))