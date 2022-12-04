package main

import (
	"encoding/json"
	"fmt"

	"github.com/hyperledger/fabric-chaincode-go/shim"
	sc "github.com/hyperledger/fabric-protos-go/peer"
)

// Define the Smart Contract structure
type SmartContract struct {
}

/*
 * The Invoke method is called as a result of an application request to run the Smart Contract.
 * The calling application program has also specified the particular smart contract function to be called, with arguments
 */
func (s *SmartContract) Invoke(APIstub shim.ChaincodeStubInterface) sc.Response {

	// Retrieve the requested Smart Contract function and arguments
	function, args := APIstub.GetFunctionAndParameters()
	// Route to the appropriate handler function to interact with the ledger appropriately
	if function == "store" {
		return s.store(APIstub, args)
	} else if function == "Init" {
		return s.Init(APIstub)
	} else if function == "createNewDocument" {
		return s.createNewDocument(APIstub, args)
	} else {
		fmt.Println("Invalid Smart Contract function name.")
		return shim.Error("Invalid Smart Contract function name.")
	}
}

type unit struct {
	Value string `json:"value"`
}

func (s *SmartContract) createNewDocument(APIstub shim.ChaincodeStubInterface, args []string) sc.Response {
	privateDocumentStore := args[0]

	type documentTransientInput struct {
		DocumentID      string `json:"documentID"` //the fieldtags are needed to keep case from bouncing around
		DocumentContent string `json:"documentcontent"`
	}

	transMap, err := APIstub.GetTransient()
	if err != nil {
		return shim.Error("Error getting transient: " + err.Error())
	}

	if _, ok := transMap["document"]; !ok {
		return shim.Error("document must be a key in the transient map")
	}

	if len(transMap["document"]) == 0 {
		return shim.Error("document value in the transient map must be a non-empty JSON string")
	}

	var documentInput documentTransientInput
	err = json.Unmarshal(transMap["document"], &documentInput)
	if err != nil {
		return shim.Error("Failed to decode JSON of: " + string(transMap["document"]))
	}

	// ==== Check if document already exists ====
	documentAsBytes, err := APIstub.GetPrivateData(privateDocumentStore, documentInput.DocumentID)
	if err != nil {
		return shim.Error("Failed to get document: " + err.Error())
	} else if documentAsBytes != nil {
		fmt.Println("This document already exists: " + documentInput.DocumentID)
		return shim.Error("This document already exists: " + documentInput.DocumentID)
	}

	// === Save document to state ===
	err = APIstub.PutPrivateData(privateDocumentStore, documentInput.DocumentID, []byte(documentInput.DocumentContent))
	if err != nil {
		return shim.Error(err.Error())
	}

	return shim.Success(nil)

}

func (s *SmartContract) store(APIstub shim.ChaincodeStubInterface, args []string) sc.Response {

	APIstub.PutState(args[0], []byte(args[1]))

	return shim.Success(nil)
}

func (s *SmartContract) Init(APIstub shim.ChaincodeStubInterface) sc.Response {
	return shim.Success(nil)
}

func main() {
	//Create a new Smart Contract
	err := shim.Start(new(SmartContract))
	if err != nil {
		fmt.Printf("Error creating new Smart Contract: %s", err)
	}
}
