package main


import (
	"C"
	"fmt"
	"os/exec"
	"encoding/json"
	"bufio"
	"time"
)

type change struct {
	Type string
	Schema string
	Name string
	Change string
	Data json.RawMessage
}

//export PgKafkaWorker
//noinspection GoUnusedExportedFunction
func PgKafkaWorker()  {
	fmt.Println("PG worker running")

	cmd := exec.Command("/usr/bin/pg_recvlogical",  "--create-slot", "--if-not-exists", "--start", "-f", "-", "-S", "kafka_events", "-P", "decoding_json", "-d", "postgres")

	out, err := cmd.StdoutPipe()
	if err != nil {
		panic(err.Error())
	}

	if err := cmd.Start(); err != nil {
		fmt.Printf("Failed to fork recvlogical: %s\n", err.Error())
		return
	}

	fmt.Println("PG worker scanning")
	scanner := bufio.NewScanner(out)
	for scanner.Scan() {
		fmt.Println("PG scanned")
		fmt.Println(scanner.Text())
	}
	//
	//decoder := json.NewDecoder(reader)
	//for {
	//	fmt.Println("PG worker decoding")
	//	c := change{}
	//	if err = decoder.Decode(&c); err != nil {
	//		fmt.Printf("Failed to read json: %s\n", err.Error())
	//		return
	//	}
	//	fmt.Printf("Parsed %s\n", c)
	//}
}

func main() {
	// needed for compilation only
	PgKafkaWorker()
	time.Sleep(30 * time.Minute)
}
