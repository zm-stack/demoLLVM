/*
 * Copyright IBM Corp All Rights Reserved
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package main

import (
	"encoding/json"
	"fmt"
	"strconv"
	"strings"

	"github.com/hyperledger/fabric-chaincode-go/shim"
	"github.com/hyperledger/fabric-protos-go/peer"
)

// SimpleAsset implements a simple chaincode to manage an asset
type SimpleAsset struct {
}

func GetKeyHisLog(stub shim.ChaincodeStubInterface, key []string) peer.Response {
	historyIter, err := stub.GetHistoryForKey(key[0])

	if err != nil {
		fmt.Println("Error: cannnot get GetHistoryForKey!")
		return shim.Error("Error: cannnot get GetHistoryForKey!")
	}

	for {
		if historyIter.HasNext() {
			modification, err := historyIter.Next()
			if err != nil {
				fmt.Println("Error: cannnot get Iter next for GetKeyHisLog!")
				return shim.Error("Error: cannnot get Iter next for GetKeyHisLog!")
			}
			fmt.Println("Returning information related to", string(modification.Value))
		} else {
			break
		}
	}
	return shim.Success(nil)
}

// Init is called during chaincode instantiation to initialize any
// data. Note that chaincode upgrade also calls this function to reset
// or to migrate data.
func (t *SimpleAsset) Init(stub shim.ChaincodeStubInterface) peer.Response {
	// Get the args from the transaction proposal
	args := stub.GetStringArgs()
	if len(args) != 2 {
		return shim.Error("Incorrect arguments. Expecting a key and a value")
	}

	// Set up any variables or assets here by calling stub.PutState()

	// We store the key and the value on the ledger
	err := stub.PutState(args[0], []byte(args[1]))
	if err != nil {
		return shim.Error(fmt.Sprintf("Failed to create asset: %s", args[0]))
	}

	fmt.Printf("TX ID: %s, BLOCK ID: %s\n", stub.GetTxID())

	return shim.Success(nil)
}

// Invoke is called per transaction on the chaincode. Each transaction is
// either a 'get' or a 'set' on the asset created by Init function. The Set
// method may create a new asset by specifying a new key-value pair.
func (t *SimpleAsset) Invoke(stub shim.ChaincodeStubInterface) peer.Response {
	// Extract the function and args from the transaction proposal
	fn, args := stub.GetFunctionAndParameters()

	var result string
	var err error
	if fn == "set" {
		result, err = set(stub, args)
	} else if fn == "get" { // assume 'get' even if fn is nil
		result, err = get(stub, args)
	} else if fn == "delete" {
		result, err = delete(stub, args)
	} else if fn == "set_table" {
		result, err = set_table_by_multi_index(stub, args)
	} else if fn == "set_table_multi_key" {
		result, err = set_table_by_multi_keywords(stub, args)
	} else if fn == "get_table" {
		result, err = get_table_by_multi_index(stub, args)
	} else if fn == "get_table_multi_key" {
		result, err = get_table_by_multi_keywords(stub, args)
	} else if fn == "delete_table" {
		result, err = del_table_by_multi_index(stub, args)
	} else if fn == "GetKeyHisLog" {
		GetKeyHisLog(stub, args)
	} else if fn == "insert_or_modify" {
		result, err = insert_or_modify(stub, args)
	} else if fn == "get_table_private" {
		result, err = get_table_record(stub, args)
	}

	if err != nil {
		return shim.Error(err.Error())
	}

	fmt.Printf("TX ID: %s, fn: %s, result: %s\n", stub.GetTxID(), fn, result)

	// Return the result as success payload
	return shim.Success([]byte(result))
}

// Set stores the asset (both key and value) on the ledger. If the key exists,
// it will override the value with the new one
func set(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 2 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key and a value")
	}

	err := stub.PutState(args[0], []byte(args[1]))
	if err != nil {
		return "", fmt.Errorf("Failed to set asset: %s", args[0])
	}
	return args[1], nil
}

func delete(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 1 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key and a value")
	}

	err := stub.DelState(args[0])
	if err != nil {
		return "", fmt.Errorf("Failed to set asset: %s", args[0])
	}
	return args[0], nil
}

// Get returns the value of the specified asset key
func get(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 1 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key")
	}

	value, err := stub.GetState(args[0])
	if err != nil {
		return "", fmt.Errorf("Failed to get asset: %s with error: %s", args[0], err)
	}
	if value == nil {
		return "", fmt.Errorf("Asset not found: %s", args[0])
	}
	return string(value), nil
}

func set_table_by_multi_index(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 3 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key and a value")
	}

	//arg[0]----table name
	//arg[1] primate key
	//arg[2...] record

	compositKey, err := stub.CreateCompositeKey(args[0], []string{args[1]})
	if err != nil {
		return "", fmt.Errorf("CreateCompositeKey failed: %s", err.Error())
	}

	err = stub.PutState(compositKey, []byte(args[2]))
	if err != nil {
		return "", fmt.Errorf("Failed to set asset: %s", args[0])
	}
	//fmt.Printf("set_table_by_multi_index: table: %s, key: %s, value: %s\n", args[0], args[1], args[2])
	return args[2], nil
}

func del_table_by_multi_index(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 2 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key and a value")
	}

	//arg[0]----table name
	//arg[1] primate key
	//arg[2...] record

	compositKey, err := stub.CreateCompositeKey(args[0], []string{args[1]})
	if err != nil {
		return "", fmt.Errorf("CreateCompositeKey failed: %s", err.Error())
	}

	err = stub.DelState(compositKey)
	if err != nil {
		return "", fmt.Errorf("Failed to set asset: %s", args[0])
	}
	//fmt.Printf("del_table_by_multi_index: table: %s, key: %s\n", args[0], args[1])
	return args[1], nil
}

func set_table_by_multi_keywords(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) < 4 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key and a value")
	}

	//arg[0]----table name
	//arg[1]----keyword numbers
	//arg[1...N] primate keys
	//arg[N+1...] record

	table_name := args[0]

	keyword := []string{}

	numbers, err := strconv.Atoi(args[1])

	if err != nil {
		return "", err
	}

	// Get all deltas for the variable
	arg_no := 2
	if numbers > 0 {
		for i := 0; i < numbers; i++ {
			keyword = append(keyword, args[arg_no])
			arg_no += 1
		}
	}

	compositKey, err := stub.CreateCompositeKey(table_name, keyword)
	if err != nil {
		return "", fmt.Errorf("CreateCompositeKey failed: %s", err.Error())
	}

	err = stub.PutState(compositKey, []byte(args[arg_no]))
	if err != nil {
		return "", fmt.Errorf("Failed to set asset: %s", args[0])
	}
	return args[2], nil
}

type Table_Data struct {
	Key   string `json:"key"`
	Value string `json:"value"`
}

// Get returns the value of the specified asset key
func get_table_by_multi_index(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 2 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key")
	}

	//arg[0]----table name
	//arg[1] primate key
	//arg[2...] record

	name := args[1]
	keyword := []string{}

	// Get all deltas for the variable
	if name != "" && "" != strings.Replace(name, " ", "", -1) {
		keyword = []string{name}
	}

	//fmt.Printf("get_table_by_multi_index 1 : table: %s, key: %s\n", args[0], keyword)
	deltaResultsIterator, deltaErr := stub.GetStateByPartialCompositeKey(args[0], keyword)
	if deltaErr != nil {
		return "", fmt.Errorf(fmt.Sprintf("Could not retrieve value for %s: %s", name, deltaErr.Error()))
	}
	defer deltaResultsIterator.Close()

	/*compositKey, err := stub.CreateCompositeKey(args[0], []string{args[1]})
	 if err != nil {
		 return "", fmt.Errorf("CreateCompositeKey failed: %s", err.Error())
	 }

	 //stub.GetQueryResult===> only CounchDB support.
	 deltaResultsIterator, deltaErr := stub.GetQueryResult(compositKey)
	 if deltaErr != nil {
		 return "", fmt.Errorf(fmt.Sprintf("Could not retrieve value for %s: %s", name, deltaErr.Error()))
	 }
	 defer deltaResultsIterator.Close()
	*/

	// Check the variable existed
	if !deltaResultsIterator.HasNext() {
		return "", fmt.Errorf(fmt.Sprintf("No variable by the name %s exists", args[1]))
	}
	// Iterate through result set and compute final value
	var i int
	var table []Table_Data
	var marshalBytes []byte
	for i = 0; deltaResultsIterator.HasNext(); i++ {
		// Get the next row
		responseRange, nextErr := deltaResultsIterator.Next()
		if nextErr != nil {
			return "", fmt.Errorf(nextErr.Error())
		}

		// Split the composite key into its component parts
		_, keyParts, splitKeyErr := stub.SplitCompositeKey(responseRange.Key)
		if splitKeyErr != nil {
			return "", fmt.Errorf(splitKeyErr.Error())
		}

		table = append(table, Table_Data{keyParts[0], string(responseRange.Value)})

		marshalBytes, _ = json.Marshal(&table)
	}

	return string(marshalBytes), nil
}

// Get returns the value of the specified asset key
func get_table_by_multi_keywords(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) < 3 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key")
	}

	//arg[0]----table name
	//arg[1] ---primary key numbers
	//arg[2..N] primary multi-keys

	table_name := args[0]

	keyword := []string{}

	if args[1] != "" && "" != strings.Replace(args[1], " ", "", -1) {

		numbers, err := strconv.Atoi(args[1])

		if err != nil {
			return "", err
		}

		// Get all deltas for the variable
		if numbers > 0 {
			arg_no := 2
			for i := 0; i < numbers; i++ {
				keyword = append(keyword, args[arg_no])
				arg_no += 1
			}
		}
	}

	deltaResultsIterator, deltaErr := stub.GetStateByPartialCompositeKey(table_name, keyword)
	if deltaErr != nil {
		return "", fmt.Errorf(fmt.Sprintf("Could not retrieve value for %s: %s", table_name, deltaErr.Error()))
	}
	defer deltaResultsIterator.Close()

	/*compositKey, err := stub.CreateCompositeKey(args[0], []string{args[1]})
	 if err != nil {
		 return "", fmt.Errorf("CreateCompositeKey failed: %s", err.Error())
	 }

	 //stub.GetQueryResult===> only CounchDB support.
	 deltaResultsIterator, deltaErr := stub.GetQueryResult(compositKey)
	 if deltaErr != nil {
		 return "", fmt.Errorf(fmt.Sprintf("Could not retrieve value for %s: %s", name, deltaErr.Error()))
	 }
	 defer deltaResultsIterator.Close()
	*/

	// Check the variable existed
	if !deltaResultsIterator.HasNext() {
		return "", fmt.Errorf(fmt.Sprintf("No variable by the name %s exists", args[1]))
	}
	// Iterate through result set and compute final value
	var i int
	var table []Table_Data
	var marshalBytes []byte
	for i = 0; deltaResultsIterator.HasNext(); i++ {
		// Get the next row
		responseRange, nextErr := deltaResultsIterator.Next()
		if nextErr != nil {
			return "", fmt.Errorf(nextErr.Error())
		}

		// Split the composite key into its component parts
		_, keyParts, splitKeyErr := stub.SplitCompositeKey(responseRange.Key)
		if splitKeyErr != nil {
			return "", fmt.Errorf(splitKeyErr.Error())
		}

		keystr := ""
		for _, item := range keyParts {
			if len(keystr) > 0 {
				keystr += ","
			}
			keystr += string(item)
		}
		table = append(table, Table_Data{keystr, string(responseRange.Value)})

		marshalBytes, _ = json.Marshal(&table)
	}

	return string(marshalBytes), nil
}

///////////////FOR ORG1+ORG2 only, private data collections ///////////////////////////////////

// Set stores the asset (both key and value) on the ledger. If the key exists,
// it will override the value with the new one
func insert_or_modify(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 3 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key and a value, args: %s", args)
	}

	//arg[0]----table name
	//arg[1] primate key
	//arg[2...] record

	err := stub.PutPrivateData(args[0], args[1], []byte(args[2]))
	//PutPrivateData(collection string, key string, value []byte) error
	if err != nil {
		return "", fmt.Errorf("Failed to set PutPrivateData's asset. err:%s---> %s %s", err.Error(), args[0], args[1])
	}
	return args[1], nil
}

func get_table_record(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 2 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key and a value")
	}

	//arg[0]----table name
	//arg[1] primate key
	//arg[2...] record

	value, err := stub.GetPrivateData(string(args[0]), string(args[1]))
	//PutPrivateData(collection string, key string, value []byte) error
	if err != nil {
		return "", fmt.Errorf("Failed to get PutPrivateData's asset: %s %s, err: %s", args[0], args[1], err.Error())
	}

	return string(value), nil
}

// main function starts up the chaincode in the container during instantiate
func main() {
	if err := shim.Start(new(SimpleAsset)); err != nil {
		fmt.Printf("Error starting SimpleAsset chaincode: %s", err)
	}
}
