package main


import (
	"C"
	"fmt"
)

//export PgKafkaWorker
//noinspection GoUnusedExportedFunction
func PgKafkaWorker()  {
	fmt.Println("PG worker running")
}

func main() {
	// needed for compilation only
}
