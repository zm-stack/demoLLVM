/*
 * Copyright IBM Corp All Rights Reserved
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package main

import (
	"encoding/json"
	"fmt"

	"github.com/hyperledger/fabric-chaincode-go/shim"
	"github.com/hyperledger/fabric-protos-go/peer"
)

// SimpleAsset implements a simple chaincode to manage an asset
type SimpleAsset struct {
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
	} else if fn == "setPrivate" {
		result, err = setPrivate(stub, args)
	} else if fn == "setTransient" {
		result, err = setTransient(stub, args)
	} else if fn == "setPrivateTransient" {
		result, err = setPrivateTransient(stub, args)
	} else if fn == "getPrivate" {
		result, err = getPrivate(stub, args)
	} else { // assume 'get' even if fn is nil
		result, err = get(stub, args)
	}
	if err != nil {
		return shim.Error(err.Error())
	}

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

func setPrivate(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 2 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key and a value")
	}

	err := stub.PutPrivateData("_implicit_org_Org1MSP", args[0], []byte(args[1]))
	if err != nil {
		return "", fmt.Errorf("Failed to set asset: %s", args[0])
	}
	return args[1], nil
}

func setTransient(stub shim.ChaincodeStubInterface, args []string) (string, error) {

	type keyValueTransientInput struct {
		Key   string `json:"key"`
		Value string `json:"value"`
	}

	if len(args) != 0 {
		return "", fmt.Errorf("Incorrect arguments. Expecting no data when using transient")
	}

	transMap, err := stub.GetTransient()
	if err != nil {
		return "", fmt.Errorf("Failed to get transient")
	}

	// assuming only "name" is processed
	keyValueAsBytes, ok := transMap["keyvalue"]
	if !ok {
		return "", fmt.Errorf("key must be keyvalue")
	}

	var keyValueInput keyValueTransientInput
	err = json.Unmarshal(keyValueAsBytes, &keyValueInput)
	if err != nil {
		return "", fmt.Errorf("Failed to decode JSON")
	}

	err = stub.PutState(keyValueInput.Key, []byte(keyValueInput.Value))
	if err != nil {
		return "", fmt.Errorf("Failed to set asset")
	}
	return keyValueInput.Value, nil
}

func setPrivateTransient(stub shim.ChaincodeStubInterface, args []string) (string, error) {

	type keyValueTransientInput struct {
		Key   string `json:"key"`
		Value string `json:"value"`
	}

	if len(args) != 0 {
		return "", fmt.Errorf("Incorrect arguments. Expecting no data when using transient")
	}

	transMap, err := stub.GetTransient()
	if err != nil {
		return "", fmt.Errorf("Failed to get transient")
	}

	// assuming only "name" is processed
	keyValueAsBytes, ok := transMap["keyvalue"]
	if !ok {
		return "", fmt.Errorf("key must be keyvalue")
	}

	var keyValueInput keyValueTransientInput
	err = json.Unmarshal(keyValueAsBytes, &keyValueInput)
	if err != nil {
		return "", fmt.Errorf("Failed to decode JSON")
	}

	err = stub.PutPrivateData("_implicit_org_Org1MSP", keyValueInput.Key, []byte(keyValueInput.Value))
	if err != nil {
		return "", fmt.Errorf("Failed to set asset")
	}
	return keyValueInput.Value, nil
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

// Get returns the value of the specified asset key
func getPrivate(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 1 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key")
	}

	value, err := stub.GetPrivateData("_implicit_org_Org1MSP", args[0])
	if err != nil {
		return "", fmt.Errorf("Failed to get asset: %s with error: %s", args[0], err)
	}
	if value == nil {
		return "", fmt.Errorf("Asset not found: %s", args[0])
	}
	return string(value), nil
}

// main function starts up the chaincode in the container during instantiate
func main() {
	if err := shim.Start(new(SimpleAsset)); err != nil {
		fmt.Printf("Error starting SimpleAsset chaincode: %s", err)
	}
}
