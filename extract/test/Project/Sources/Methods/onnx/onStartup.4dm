var $huggingfaces : cs:C1710.event.huggingfaces
var $huggingface : cs:C1710.event.huggingface

var $homeFolder : 4D:C1709.Folder
$homeFolder:=Folder:C1567(fk home folder:K87:24).folder(".ONNX")

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

$port:=8080

Case of 
	: (False:C215)  //decoder
		
		$folder:=$homeFolder.folder("granite-embedding-english-r2")
		$path:="granite-embedding-english-r2-onnx-int8"
		$URL:="keisuke-miyako/granite-embedding-english-r2-onnx-int8"
		
		$pooling:="cls"
		
	: (True:C214)  //decoder with context
		
		$folder:=$homeFolder.folder("pplx-embed-context-v1-0.6b")
		$path:="pplx-embed-context-v1-0.6b-onnx-int8"
		$URL:="keisuke-miyako/pplx-embed-context-v1-0.6b-onnx-int8"
		
		$pooling:="mean"
		
End case 

$options:={pooling: $pooling}

var $text : Text
$text:=(($URL="@-f16") || ($URL="@-f32")) ? "model.onnx" : "model_quantized.onnx"
$huggingface:=cs:C1710.event.huggingface.new($folder; $URL; $path; "embedding"; $text)
$huggingfaces:=cs:C1710.event.huggingfaces.new([$huggingface])

var $ONNX : cs:C1710.ONNX.ONNX
//$ONNX:=cs.ONNX.ONNX.new($port; $huggingfaces; $homeFolder; $options; $event)

$homeFolder:=Folder:C1567(fk home folder:K87:24).folder(".GGUF")

var $max_position_embeddings; $batch_size; $parallel; $threads; $batches : Integer

Case of 
	: (True:C214)  //decoder
		
		$folder:=$homeFolder.folder("harrier-oss-v1-0.6b")
		$path:="harrier-oss-v1-0.6b-Q8_0.gguf"
		$URL:="keisuke-miyako/harrier-oss-v1-0.6b-gguf-q8_0"
		
		$max_position_embeddings:=32768
		$pooling:="last"
		
	: (False:C215)  //encoder
		
		$folder:=$homeFolder.folder("granite-embedding-english-r2")
		$path:="granite-embedding-english-r2-Q8_0.gguf"
		$URL:="keisuke-miyako/granite-embedding-english-r2-gguf-q8_0"
		
		$max_position_embeddings:=8192
		$pooling:="cls"
		
End case 

$batch_size:=$max_position_embeddings
$batches:=1
$threads:=8  // M1 Pro P-cores; don't derive this dynamically

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
log_file: $logFile; \
ctx_size: $max_position_embeddings*$batches; \
batch_size: $batch_size; \
ubatch_size: 2048; \
parallel: $batches; \
threads: $threads; \
threads_batch: $threads; \
threads_http: 2; \
log_disable: False:C215; \
n_gpu_layers: 0}

$huggingface:=cs:C1710.event.huggingface.new($folder; $URL; $path)
$huggingfaces:=cs:C1710.event.huggingfaces.new([$huggingface])

var $llama : cs:C1710.llama.llama
$llama:=cs:C1710.llama.llama.new($port; $huggingfaces; $homeFolder; $options; $event)