![version](https://img.shields.io/badge/version-21%2B-3B69E9)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-extract)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-extract/total)

# 4d-plugin-extract
Universal Document Parser

## Abstract

Extract text from various document types in a chunked format which can be passed directly to one of the following endpoints:

- `embeddings`
- `contextualizedembeddings`

## Extract SET OPTION

Loads a GGUF file without tensors. Using the same tokeniser as llama.cpp allows the plugin to generate chunks of text with the exact token count.

```4d
Extract SET OPTION(Extract Option Tokenizer File; $file)
```

## Extract

This is the main function. Pass the document type, output format, and a `task` object.

```4d
$task:={file: $file; \
	text_as_tokens: False; \
	tokens_length: 1022; \
	overlap_ratio: 0.09; \
	unique_values_only: True; \
	pooling_mode: Extract Pooling Mode Mean}
	$extracted:=Extract(Extract Document DOCX; Extract Output Collection; $task)
```

### Supported Document Types

|File Extension|Constant|Value
|-|-|-:|
|xlsx|`Extract Document XLSX`| `0`|
|docx|`Extract Document DOCX`| `1`|
|pptx|`Extract Document PPTX`| `2`|
|xls |`Extract Document XLS` | `3`|
|doc |`Extract Document DOC` | `4`|
|ppt |`Extract Document PPT` | `5`|
|pdf |`Extract Document PDF` | `6`|
|msg |`Extract Document MSG` | `7`|
|eml|||
|rtf |`Extract Document RTF` | `9`|
|html|`Extract Document HTML`|`10`|
|txt |`Extract Document TXT` |`11`|


### Supported Output Formats

|Constant|Value|Description
|:-|-:|-
|`Extract Output Object`|`0`|For custom processing, focus on structure
|`Extract Output Text`|`1`|For custom processing, focus on text content
|`Extract Output Collection`|`2`|Suitable for **OpenAI** style `embeddings` API 
|`Extract Output Collections`|`3`|Suitable for **Voyage AI** style `contextualizedembeddings` API 

### Extract Pooling Mode

|File Extension|Constant|Value
|-|-|-:|
|mean|`Extract Pooling Mode Mean`|`0`|
|cls|`Extract Pooling Mode CLS`|`1`|
|last|`Extract Pooling Mode Last`|`2`|

### Output Options

|Option|Description
|-|-|
|`password`||Password to open DOCX, XLSX, PPTX
|`charset`||Charset to open XLS
|`text_as_tokens`|Return chunks as collection of token IDs instead of text (for debug)
|`unique_values_only`|Skip duplicate values at row or paragraph/column level (default:`false`)
|`max_paragraph_length`|Limit paragraphs per page/slide ( default:`-1`)
|`tokens_length`|Limit tokens per chunks (default:`1024`)
|`token_padding`|Pad last chunk to fixed token count (default:`false`)
|`pooling_mode`|If `Lasr` prepend else append for token padding (default:`Mean`)
|`overlap_ratio`|Overlap tokens between chunks (default:`0.09`)

> [!TIP]
> For decoder-only models, set `pooling_mode` to `Last` and `token_padding` to `true`. For encoder-only models, set `token_padding` to `false`. 

#### `Extract Output Text`

- `input`: The entire document text concatenated.
- `documents`: The document divided into semantic chunks. Same as the `input` collection as `Extract Output Collection`

#### `Extract Output Collection`  

- `input`: The document divided into semantic chunks. Use `unique_values_only` and `max_paragraph_length` to control sampling rules.

#### `Extract Output Collections`  

- `inputs`: The document divided into chunks of semantic chunks. Use `unique_values_only` and `max_paragraph_length` to control sampling rules.

## Harrier OSS v1.0 230m

|Parameters|Dimensions|Hidden Layers|`tokenizer.ggml.model`|`n_ctx_train`|`pooling`
|-:|-:|-:|-:|-:|:-:
|`268098816`|`640`|`18`|`gpt2`|`4096`|`last`

> [!WARNING]
> `ubatch_size` must be large enough to store `max_position_embeddings`.

#### Q8_0

Split into fixed size batches of `1024` tokens each

|Tokens|GPU Layers:0|
|-:|-:|
|`30720`|`13.8`|
|`12288`|`5.6`|
|`6144`|`2.7`|
|`2048`|`0.9`|
|`1024`|`0.4`|

## Harrier OSS v1.0 0.6b

|Parameters|Dimensions|Hidden Layers|`tokenizer.ggml.model`|`n_ctx_train`|`pooling`
|-:|-:|-:|-:|-:|:-:
|`596049920`|`1024`|`28`|`gpt2`|`32768`|`last`

> [!WARNING]
> GPU offloading eases the CPU but splits the graph in half, i.e. `58` ping-pongs. **Stay on CPU**.

#### Q4_k_m

Split into fixed size batches of `1024` tokens each

|Tokens|GPU Layers:0|
|-:|-:|
|`19456`|`47.8`|
|`4096`|`10.5`|
|`1024`|`1.3`|

> [!NOTE]
> Due to token-padding, chunks will always be multiples of fixed number.

## Nomic Embed Text v1.5

|Parameters|Dimensions|Hidden Layers|`tokenizer.ggml.model`|`n_ctx_train`|`pooling`
|-:|-:|-:|-:|-:|:-:
|`136727040`|`768`|`12`|`t5`|`2048`|`mean`|

#### Q8_0

Split into fixed size batches of `1024` tokens each

|Tokens|GPU Layers:12|
|-:|-:|
|`18015`|`1.6`|
|`16827`|`1.5`|
|`6844`|`0.6`|
|`3421`|`0.3`|
|`578`|`0.05`|
|`411`|`0.03`|
|`197`|`0.02`|

## EmbeddingGemma 300m

|Parameters|Dimensions|Hidden Layers|`tokenizer.ggml.model`|`n_ctx_train`|`pooling`
|-:|-:|-:|-:|-:|:-:|
|`302863104`|`768`|`24`|`llama`|`2048`|`mean`|

#### Q8_0

Split into fixed size batches of `1024` tokens each

|Tokens|GPU Layers:24|
|-:|-:|
|`21119`|`1.9`|
|`19398`|`1.8`|
|`7151`|`0.7`|
|`3576`|`0.3`|
|`618`|`0.06`|
|`508`|`0.05`|
|`415`|`0.04`|
|`305`|`0.03`|
|`217`|`0.02`|

---

- `15` seconds * `1` million documents = `173.61` days
- `1` second * `1` million documents = `11.57` days
- `0.1` seconds * `1` million documents = `1.15` days
- `0.05` seconds * `1` million documents = `13.89` hours

 To generate a batch of long context embeddings it is essential to **rent a GPU cluster**. Processing millions of document locally on a standard PC with a decoder-only model like Harrier or Qwen3 would take months.

- [**Modal**](https://modal.com) - Serverless; auto-scaling large batches
- [**Runpod**](https://www.runpod.io) - Serverless; quick prototyping and medium batches
- [**Lambda**](https://lambda.ai) - Dedicated VM; long-running jobs or sensitive data

---

In any case, you would need a reranker to prune the initial fetch by embeddings.

```4d
var $query : Text
$query:="4D Write Pro AI features summit 2021"

var $AIClient : cs.AIKit.OpenAI
$AIClient:=cs.AIKit.OpenAI.new()
$AIClient.baseURL:="http://127.0.0.1:8080/v1"  // embeddings

var $batch : Object
$batch:=$AIClient.embeddings.create($query)

var $reranked : Collection
$reranked:=[]

If ($batch.success)
	$vector:=$batch.embedding.embedding
	
	/*
		fetch matching documents; allow some false positives
	*/
	
	var $comparison:={vector: $vector; metric: mk cosine; threshold: 0.6}
	var $results:=ds.Embeddings.query("embedding > :1"; $comparison)
	If ($results.length#0)
		
		/*
			get related document
		*/
		
		$documents:=$results.text
		
		/*
			now rerank the n best results relevant to query
		*/
		
		var $client:=cs.AIKit.Reranker.new({baseURL: "http://127.0.0.1:8081/v1"})
		var $reranker:=cs.AIKit.RerankerQuery.new({\
		query: $query; documents: $documents})
		var $parameters:=cs.AIKit.RerankerParameters.new({model: "default"; top_n: 3})
		
		$batch:=$client.rerank.create($reranker; $parameters)
		
		If ($batch.success)
			var $rankings : Collection
			$rankings:=$batch.results
			var $ranking : Object
			var $rankedresults : Collection
			$rankedresults:=[]
			var $ee : cs.EmbeddingsEntity
			For each ($ranking; $rankings)
				$ee:=$results.at($ranking.index)
				$rankedresults.push({embeddings: $ee; relevance_score: $ranking.relevance_score})
			End for each 
			$rankedresults:=$rankedresults.extract(\
			"embeddings.document.ID"; "document"; \
			"embeddings.text"; "text"; \
			"relevance_score"; "score")
			ALERT(JSON Stringify($rankedresults; *))
		End if 
	End if 
End if
```

