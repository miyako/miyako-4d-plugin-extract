//%attributes = {}
var $query : Text
$query:="Instruct: Retrieve semantically similar text\nQuery: thibaud and josh talk about future of 4D"

var $AIClient : cs:C1710.AIKit.OpenAI
$AIClient:=cs:C1710.AIKit.OpenAI.new()
$AIClient.baseURL:="http://127.0.0.1:8080/v1"  // embeddings

var $batch : Object
$batch:=$AIClient.embeddings.create($query)

var $reranked : Collection
$reranked:=[]

If ($batch.success)
	$vector:=$batch.embedding.embedding
	var $comparison:={vector: $vector; metric: mk cosine:K95:1; threshold: 0.45}
	var $results:=ds:C1482.Embeddings.query("embedding > :1"; $comparison)
	If ($results.length#0)
		
		$documents:=$results.document.extracted.extract("documents")
		
		ALERT:C41(JSON Stringify:C1217($documents; *))
		
		//TODO: reranker
		
	End if 
End if 