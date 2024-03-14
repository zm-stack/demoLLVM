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

	if fn == "SetRecord1" {
		result, err = SetRecord1(stub, args)
	} else if fn == "GetRecord" {
		result, err = GetRecord(stub, args)
	} else if fn == "SetRecord" {
		result, err = SetRecord(stub, args)
	}
	if err != nil {
		return shim.Error(err.Error())
	}

	// Return the result as success payload
	return shim.Success([]byte(result))
}

func SetRecord1(stub shim.ChaincodeStubInterface, args []string) (string, error) {
	if len(args) != 2 {
		return "", fmt.Errorf("Incorrect arguments. Expecting a key and a value")
	}

	err := stub.PutPrivateData("_implicit_org_Org1MSP", args[0], []byte(args[1]))
	if err != nil {
		return "", fmt.Errorf("Failed to set asset: %s", args[0])
	}
	return args[1], nil
}

// Get returns the value of the specified asset key
func GetRecord(stub shim.ChaincodeStubInterface, args []string) (string, error) {
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

func SetRecord(stub shim.ChaincodeStubInterface, args []string) (string, error) {

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

// main function starts up the chaincode in the container during instantiate
func main() {
	if err := shim.Start(new(SimpleAsset)); err != nil {
		fmt.Printf("Error starting  chaincode: %s", err)
	}
}

// export KEYVALUE=$(echo -n "{\"key\":\"name2\",\"value\":\"charlie2\"}" | base64 | tr -d \\n)
// docker exec cli peer chaincode invoke -o orderer.example.com:7050 --tls true --cafile /opt/gopath/src/github.com/hyperledger/fabric/peer/crypto/ordererOrganizations/example.com/orderers/orderer.example.com/msp/tlscacerts/tlsca.example.com-cert.pem -C mychannel -n private_record --peerAddresses peer0.org1.example.com:9051 --tlsRootCertFiles /opt/gopath/src/github.com/hyperledger/fabric/peer/crypto/peerOrganizations/org1.example.com/peers/peer0.org1.example.com/tls/ca.crt -c '{"Args":["SetRecord"]}' --transient "{\"keyvalue\":\"$KEYVALUE\"}"
