//%attributes = {}
var $query : Text
$query:="Instruct: Retrieve semantically similar text\nQuery: thibaud and josh talk about future of 4D"

var $AIClient : cs:C1710.AIKit.OpenAI
$AIClient:=cs:C1710.AIKit.OpenAI.new()
$AIClient.baseURL:="http://127.0.0.1:8080/v1"  // embeddings

var $batch : Object
$batch:=$AIClient.embeddings.create($query)

If ($batch.success)
	$vector:=$batch.embedding.embedding
	
/*
fetch matching documents; allow some false positives
*/
	
	var $comparison:={vector: $vector; metric: mk cosine:K95:1; threshold: 0.6}
	var $results:=ds:C1482.Embeddings.query("embedding > :1"; $comparison)
	If ($results.length#0)
		
/*
get related document
*/
		
		$documents:=$results.text
		
/*
now rerank the n best results relevant to query
*/
		
		var $client:=cs:C1710.AIKit.Reranker.new({baseURL: "http://127.0.0.1:8081/v1"})
		var $reranker:=cs:C1710.AIKit.RerankerQuery.new({\
			query: $query; documents: $documents})
		var $parameters:=cs:C1710.AIKit.RerankerParameters.new({model: "default"; top_n: 3})
		
		$batch:=$client.rerank.create($reranker; $parameters)
		
		If ($batch.success)
			var $rankings : Collection
			$rankings:=$batch.results
			var $ranking : Object
			var $rankedresults : Collection
			$rankedresults:=[]
			var $ee : cs:C1710.EmbeddingsEntity
			For each ($ranking; $rankings)
				$ee:=$results.at($ranking.index)
				$rankedresults.push({embeddings: $ee; relevance_score: $ranking.relevance_score})
			End for each 
			$rankedresults:=$rankedresults.extract(\
				"embeddings.document.ID"; "document"; \
				"embeddings.text"; "text"; \
				"relevance_score"; "score")
			ALERT:C41(JSON Stringify:C1217($rankedresults; *))
		End if 
	End if 
End if 