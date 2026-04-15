var $llama : cs:C1710.llama.llama
var $huggingfaces : cs:C1710.event.huggingfaces
var $embeddings; $rerank : cs:C1710.event.huggingface
var $homeFolder : 4D:C1709.Folder

var $file : 4D:C1709.File
var $URL : Text
var $port : Integer

var $event : cs:C1710.event.event
$event:=cs:C1710.event.event.new()

$event.onError:=Formula:C1597(OnModelDownloaded)
$event.onSuccess:=Formula:C1597(OnModelDownloaded)

$event.onData:=Formula:C1597(LOG EVENT:C667(Into 4D debug message:K38:5; This:C1470.file.fullName+":"+String:C10((This:C1470.range.end/This:C1470.range.length)*100; "###.00%")))
//$event.onData:=Formula(MESSAGE(This.file.fullName+":"+String((This.range.end/This.range.length)*100; "###.00%")))
$event.onResponse:=Formula:C1597(LOG EVENT:C667(Into 4D debug message:K38:5; This:C1470.file.fullName+":download complete"))
//$event.onResponse:=Formula(MESSAGE(This.file.fullName+":download complete"))
$event.onTerminate:=Formula:C1597(LOG EVENT:C667(Into 4D debug message:K38:5; (["process"; $1.pid; "terminated!"].join(" "))))

$homeFolder:=Folder:C1567(fk home folder:K87:24).folder(".GGUF")
var $max_position_embeddings; $batch_size; $parallel; $threads; $batches : Integer

Case of 
	: (False:C215)  //decoder
		
		$folder:=$homeFolder.folder("harrier-oss-v1-270m")
		$path:="harrier-oss-v1-270m-Q8_0.gguf"
		$URL:="keisuke-miyako/harrier-oss-v1-270m-gguf-q8_0"
		
		$max_position_embeddings:=1536
		$pooling:="last"
		$batch_size:=$max_position_embeddings
		$ubatch_size:=1024
		$n_gpu_layers:=0
		$cache_type_k:="f16"
		$cache_type_v:="f16"
		
	: (False:C215)  //decoder
		
		$folder:=$homeFolder.folder("harrier-oss-v1-0.6b")
		$path:="harrier-oss-v1-0.6b-Q4_k_m.gguf"
		$URL:="keisuke-miyako/harrier-oss-v1-0.6b-gguf-q4_k_m"
		
		$max_position_embeddings:=1536
		$pooling:="last"
		$batch_size:=$max_position_embeddings
		$ubatch_size:=512
		$n_gpu_layers:=0
		$cache_type_k:="f16"
		$cache_type_v:="f16"
		
	: (False:C215)  //encoder
		
		$folder:=$homeFolder.folder("embeddinggemma-300m")
		$path:="embeddinggemma-300m-Q8_0.gguf"
		$URL:="keisuke-miyako/embeddinggemma-300m-gguf-q8_0"
		
		$max_position_embeddings:=1024
		$pooling:="mean"
		$batch_size:=$max_position_embeddings
		$ubatch_size:=$max_position_embeddings
		$n_gpu_layers:=24
		$cache_type_k:="f16"
		$cache_type_v:="f16"
		
	: (True:C214)  //encoder
		
		$folder:=$homeFolder.folder("nomic-embed-text-v1.5")
		$path:="nomic-embed-text-v1.5-Q8_0.gguf"
		$URL:="keisuke-miyako/nomic-embed-text-v1.5-gguf-q8_0"
		
		$max_position_embeddings:=1024
		$pooling:="mean"
		$batch_size:=$max_position_embeddings
		$ubatch_size:=$max_position_embeddings
		$n_gpu_layers:=13
		$cache_type_k:="f16"
		$cache_type_v:="f16"
		
	: (False:C215)  //encoder
		
		$folder:=$homeFolder.folder("nomic-embed-text-v2-moe")
		$path:="nomic-embed-text-v2-moe-Q8_0.gguf"
		$URL:="keisuke-miyako/nomic-embed-text-v2-moe-gguf-q8_0"
		
		$max_position_embeddings:=512
		$pooling:="mean"
		$batch_size:=$max_position_embeddings
		$ubatch_size:=$max_position_embeddings
		$n_gpu_layers:=12
		$cache_type_k:="f16"
		$cache_type_v:="f16"
		
End case 

$batches:=2  //up to 2 requests at a time
$threads:=4  // M1 Pro P-cores

var $logFile : 4D:C1709.File
$logFile:=$folder.file("llama.log")
$folder.create()
If (Not:C34($logFile.exists))
	$logFile.setContent(4D:C1709.Blob.new())
End if 

$port:=8080
$options:={\
embeddings: True:C214; \
pooling: $pooling; \
ctx_size: $max_position_embeddings*$batches; \
batch_size: $batch_size; \
ubatch_size: $ubatch_size; \
parallel: $batches; \
threads: $threads; \
threads_batch: $threads; \
threads_http: 2; \
log_file: $logFile; \
log_disable: False:C215; \
n_gpu_layers: $n_gpu_layers; \
cache_type_k: $cache_type_k; \
cache_type_v: $cache_type_v; \
flash_attn: "on"; cont_batching: True:C214}

$embeddings:=cs:C1710.event.huggingface.new($folder; $URL; $path)
$huggingfaces:=cs:C1710.event.huggingfaces.new([$embeddings])
$llama:=cs:C1710.llama.llama.new($port; $huggingfaces; $homeFolder; $options; $event)

//MARK: Reranker

/*
if the reranker has a different tokeniser from embeddings
you should account for some buffer to avoid error
*/

Case of 
	: (False:C215)  //pretty slow...
		
		$folder:=$homeFolder.folder("zerank-1-small")
		$path:="zerank-1-small-Q8_0.gguf"
		$URL:="keisuke-miyako/zerank-1-small-gguf-q8_0"
		
		$max_position_embeddings:=1600
		$pooling:="rank"
		$batch_size:=$max_position_embeddings
		$ubatch_size:=$max_position_embeddings
		$n_gpu_layers:=12
		$cache_type_k:="f16"
		$cache_type_v:="f16"
		
	: (True:C214)
		
		$folder:=$homeFolder.folder("bge-reranker-v2-m3")
		$path:="bge-reranker-v2-m3-Q8_0.gguf"
		$URL:="keisuke-miyako/bge-reranker-v2-m3-gguf-q8_0"
		
		$max_position_embeddings:=1600
		$pooling:="rank"
		$batch_size:=$max_position_embeddings
		$ubatch_size:=$max_position_embeddings
		$n_gpu_layers:=12
		$cache_type_k:="f16"
		$cache_type_v:="f16"
		
	: (False:C215)
		
		$folder:=$homeFolder.folder("mmarco-mMiniLMv2-L12-H384-v1")
		$path:="mmarco-mMiniLMv2-L12-H384-v1-Q8_0.gguf"
		$URL:="keisuke-miyako/mmarco-mMiniLMv2-L12-H384-v1-gguf-q8_0"
		
		$max_position_embeddings:=514
		$pooling:="rank"
		$batch_size:=$max_position_embeddings
		$ubatch_size:=$max_position_embeddings
		$n_gpu_layers:=12
		$cache_type_k:="f16"
		$cache_type_v:="f16"
		
End case 

$logFile:=$folder.file("llama.log")
$folder.create()
If (Not:C34($logFile.exists))
	$logFile.setContent(4D:C1709.Blob.new())
End if 

$batches:=2  //up to 2 requests at a time
$threads:=4  // M1 Pro P-cores

$port:=8081
$options:={\
reranking: True:C214; \
pooling: $pooling; \
ctx_size: $max_position_embeddings*$batches; \
batch_size: $batch_size; \
ubatch_size: $ubatch_size; \
parallel: $batches; \
threads: $threads; \
threads_batch: $threads; \
threads_http: 2; \
log_file: $logFile; \
log_disable: False:C215; \
n_gpu_layers: $n_gpu_layers; \
cache_type_k: $cache_type_k; \
cache_type_v: $cache_type_v}

$rerank:=cs:C1710.event.huggingface.new($folder; $URL; $path)
$huggingfaces:=cs:C1710.event.huggingfaces.new([$rerank])
$llama:=cs:C1710.llama.llama.new($port; $huggingfaces; $homeFolder; $options; $event)