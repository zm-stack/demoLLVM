package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"log"

	"github.com/hyperledger/fabric-chaincode-go/pkg/cid"
	"github.com/hyperledger/fabric-chaincode-go/shim"
	"github.com/hyperledger/fabric-protos-go/peer"
)

// ====CHAINCODE EXECUTION SAMPLES (CLI) ==================
// ==== Invoke coffeehub, pass private data as base64 encoded bytes in transient map ====
// export PRODUCT=$(echo -n"{\"Name\":\"Americano\",\"Type\":\"Coffee\",\"Price\":2.50}" | base64 | tr -d \\n)
// peer chaincode invoke -C channela -n coffeehubcc -c '{"Args":["initProduct"]}' --transient "{\"product\":\"$PRODUCT\"}"
//
// export PRODUCT=$(echo -n"{\"Name\":\"Latte\",\"Type\":\"Coffee,\",\"Price\":2.50}" | base64 | tr -d \\n)
// peer chaincode invoke -C channela -n coffeehubcc -c '{"Args":["initProduct"]}' --transient "{\"product\":\"$PRODUCT\"}"
//
// export PRODUCT=$(echo -n"{\"Name\":\"Cappuccino\",\"Type\":\"Coffee,\",\"Price\":2.50}" | base64 | tr -d \\n)
// peer chaincode invoke -C channela -n coffeehubcc -c '{"Args":["initProduct"]}' --transient "{\"product\":\"$PRODUCT\"}"
//
// export ORDER_PRODUCT=$(echo -n"{\"Name\":\"Cappuccino\",\"Type\":\"Coffee,\",\"Price\":2.50}" | base64 | tr -d \\n)
// export ORDER=$(echo -n "{\"Name\":\"Alice\",\"Vendor\":\"Bob's Coffees\",\"Product\":\"$PRODUCT\",\"qty\":1}" | base64 | tr -d \\n)
// peer chaincode invoke -C channela -n coffeehubcc -c '{"Args":["processOrder"]}' --transient "{\"order\":\"$ORDER\"}"
//
// ==== Query products and sales information, since queries are not recorded on chain we don't need to hide private data in transient map ====
// peer chaincode query -C channela -n coffeehubcc -c '{"Args":["getProducts"]}'
// peer chaincode query -C channela -n coffeehubcc -c '{"Args":["getTotalSales"]}'

// Steps
// - initialize product catalog by invoking initProduct
// - get all initialized products
// - create an order based on products and customer info
// - check sales data (total sales)

// VirtualCoffeeShopChaincode Chaincode implementation
type VirtualCoffeeShopChaincode struct{}

type docProduct struct {
	Key    string  `json:"Key"`
	Record product `json:"Record"`
}

type docOrder struct {
	Key    string        `json:"Key"`
	Record orderResponse `json:"Record"`
}

type plist []docProduct

type olist []docOrder

type shopRecord struct {
	Vendor string  `json:"Vendor,omitempty"`
	Olist  olist   `json:"Olist,omitempty"`
	Plist  plist   `json:"Plist,omitempty"`
	TSales float32 `json:"Tsales,omitempty"`
}

type productTransientInput struct {
	Name  string
	Type  string
	Price float32
}

type product struct {
	ObjectType string  `json:"docType"` // docType is used to distinguish the various types of objects in state database
	Name       string  `json:"Name"`
	Type       string  `json:"Type"`
	Price      float32 `json:"Price"`
}

type orderRequest struct {
	ID      string                `json:"ID"`
	Name    string                `json:"Name"`    // customer name
	Vendor  string                `json:"Vendor"`  // vendor name
	Product productTransientInput `json:"Product"` // name, type, and price
	Qty     int                   `json:"Qty"`     // order quantity
	Payment float32               `json:"Payment"` // amount paid by the customer. could be more or less than the product price
}

type orderResponse struct {
	ObjectType   string       `json:"docType"` // docType is used to distinguish the various types of objects in state database
	OrderRequest orderRequest `json:"OrderRequest"`
	Change       float32      `json:"Change"`
}

func main() {
	err := shim.Start(new(VirtualCoffeeShopChaincode))
	if err != nil {
		fmt.Printf("Error starting Simple chaincode: %s", err)
	}
}

// Init initializes chaincode
func (v *VirtualCoffeeShopChaincode) Init(stub shim.ChaincodeStubInterface) peer.Response {
	return shim.Success(nil)
}

// Invoke is the entrypoint for invocations
func (v *VirtualCoffeeShopChaincode) Invoke(stub shim.ChaincodeStubInterface) peer.Response {
	function, args := stub.GetFunctionAndParameters()
	fmt.Println("invoke is running " + function)

	switch function {
	case "initProduct":
		return v.initProduct(stub, args)
	case "getProducts":
		return v.getProducts(stub, args)
	case "getBobsProducts":
		return v.getBobsProducts(stub, args)
	case "getCharliesProducts":
		return v.getCharliesProducts(stub, args)
	case "processOrder":
		return v.processOrder(stub, args)
	case "getOKOrders":
		return v.getOKOrders(stub, args)
	case "getNOKOrders":
		return v.getNOKOrders(stub, args)
	case "getTotalSales":
		return v.getTotalSales(stub, args)
	case "getBobsTotalSales":
		return v.getBobsTotalSales(stub, args)
	case "getCharliesTotalSales":
		return v.getCharliesTotalSales(stub, args)
	case "getHash":
		return v.getHash(stub, args)
	default:
		//error
		fmt.Println("invoke did not find func: " + function)
		return shim.Error("Received unknown function invocation")
	}
}

func (v *VirtualCoffeeShopChaincode) getHash(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	var collection string
	var ok bool
	var r peer.Response

	transMap, err := stub.GetTransient()
	if err != nil {
		return shim.Error(err.Error())
	}

	type hashReq struct {
		Key string
	}

	if r, collection, ok = getMSPCollectionName(stub); !ok {
		return r
	}

	log.Printf("input string %s\n", string(transMap["hashReq"]))

	var hr hashReq
	err = json.Unmarshal(transMap["hashReq"], &hr)
	if err != nil {
		return shim.Error("Failed to decode JSON of: " + string(transMap["hashReq"]))
	}

	hashbytes, err := stub.GetPrivateDataHash(collection, hr.Key)
	if err != nil {
		return shim.Error("Failed to get Data Hash with Key: " + hr.Key)
	}

	return shim.Success(hashbytes)
}

func (v *VirtualCoffeeShopChaincode) initProduct(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[initProduct] - START")
	defer log.Printf("[initProduct] - END")
	var collection string

	r, input, ok := sanitizeInput(stub, args)
	if !ok {
		return r
	}

	var err error
	productInput := input.(*productTransientInput)
	if _, collection, ok, err = checkIfProductExists(stub, productInput); ok {
		existErr := errors.New("Product already exists")
		return shim.Error(existErr.Error())
	}
	if err != nil {
		return shim.Error(err.Error())
	}

	// Create product private details object with name, type, and price, marshal to JSON, and save to state
	return createProductPrivateDetails(stub, productInput, collection)
}

func (v *VirtualCoffeeShopChaincode) getProducts(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[getProducts] - START")
	defer log.Printf("[getProducts] - END")

	var collection string
	var r peer.Response
	var valAsbytes []byte
	var ok bool

	if len(args) > 0 {
		return shim.Error("Not expecting any arguments")
	}

	if r, collection, ok = getMSPCollectionName(stub); !ok {
		return r
	}

	if collection == "AllCollections" {
		//Bob's
		bplist := plist{}
		collection = "BobsCoffeeCollection"
		valAsbytes, r = getCollection(stub, collection, "productPrivateDetails")
		if valAsbytes == nil {
			return r
		}
		err := json.Unmarshal(valAsbytes, &bplist)
		if err != nil {
			return shim.Error(err.Error())
		}
		bobs := shopRecord{
			Vendor: "BobsCoffees",
			Plist:  bplist,
		}

		//Charlie's
		cplist := plist{}
		collection = "CharliesCoffeeCollection"
		valAsbytes, r = getCollection(stub, collection, "productPrivateDetails")
		if valAsbytes == nil {
			return r
		}
		err = json.Unmarshal(valAsbytes, &cplist)
		if err != nil {
			return shim.Error(err.Error())
		}
		charlies := shopRecord{
			Vendor: "CharliessCoffees",
			Plist:  bplist,
		}

		//combine
		pAll := struct {
			Shops []shopRecord
		}{
			[]shopRecord{bobs, charlies},
		}
		log.Printf("pall: %+v\n", pAll)
		valAsbytes, err = json.Marshal(pAll)
		if err != nil {
			return shim.Error(err.Error())
		}
		log.Printf("valAsbytes-p: %+v\n", valAsbytes)
	} else {
		valAsbytes, r = getCollection(stub, collection, "productPrivateDetails")
		if valAsbytes == nil {
			return r
		}
	}

	log.Printf("valAsbytes: %+v\n", valAsbytes)
	return shim.Success(valAsbytes)
}

func (v *VirtualCoffeeShopChaincode) getBobsProducts(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[getBobsProducts] - START")
	defer log.Printf("[getBobsProducts] - END")

	var jsonResp string

	if len(args) > 0 {
		return shim.Error("Not expecting any arguments")
	}

	collection := "BobsCoffeeCollection"

	qstring := "{\"selector\":{\"docType\":\"productPrivateDetails\"}}"
	valAsbytes, err := getQueryResultForQueryString(stub, collection, qstring)
	if err != nil {
		log.Printf("Error: %s\n", err.Error())
		jsonResp = "{\"Error\":\"Failed to get state\"}"
		return shim.Error(jsonResp + err.Error())
	} else if valAsbytes == nil {
		jsonResp = "{\"Error\":\"No products\"}"
		return shim.Error(jsonResp)
	}

	return shim.Success(valAsbytes)
}

func (v *VirtualCoffeeShopChaincode) getCharliesProducts(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[getCharliesProducts] - START")
	defer log.Printf("[getCharliesProducts] - END")

	var jsonResp string

	if len(args) > 0 {
		return shim.Error("Not expecting any arguments")
	}

	collection := "CharliesCoffeeCollection"

	qstring := "{\"selector\":{\"docType\":\"productPrivateDetails\"}}"
	valAsbytes, err := getQueryResultForQueryString(stub, collection, qstring)
	if err != nil {
		log.Printf("Error: %s\n", err.Error())
		jsonResp = "{\"Error\":\"Failed to get state\"}"
		return shim.Error(jsonResp + err.Error())
	} else if valAsbytes == nil {
		jsonResp = "{\"Error\":\"No products\"}"
		return shim.Error(jsonResp)
	}

	return shim.Success(valAsbytes)
}

func (v *VirtualCoffeeShopChaincode) processOrder(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[processOrder] - START")
	defer log.Printf("[processOrder] - END")
	var collection string
	var productOrder product
	var productBytes []byte
	var jsonResp string
	var rspBytes []byte
	var change float32
	var oType string

	err := errors.New("")

	r, input, ok := sanitizeInput(stub, args)
	if !ok {
		return r
	}

	orderInput := input.(*orderRequest)
	if productBytes, collection, ok, err = checkIfProductExists(stub, &orderInput.Product); !ok {
		err := errors.New("Product does not exists")
		return shim.Error(err.Error())
	}
	if err != nil {
		log.Printf("Error: unable to check product existence.\n")
		return shim.Error(err.Error())
	}

	log.Printf("productBytes %s\n", string(productBytes))
	err = json.Unmarshal(productBytes, &productOrder)
	if err != nil {
		log.Printf("Error: unable to unmarshal to productOrder.\n")
		return shim.Error(err.Error())
	}

	// because input does not contain price and we should always
	// get updated price from ledger
	orderInput.Product.Price = productOrder.Price

	if orderInput.Payment < productOrder.Price {
		log.Printf("Msg: received payment is insufficient.\n")
		jsonResp = "{\"Msg\":\"Received payment is insufficient.\"}"
		oType = "NOKTransactionDetails"
	} else {
		log.Printf("Msg: Payment successful.\n")
		jsonResp = "{\"Msg\":\"Payment successful.\"}"
		oType = "OkTransactionDetails"
	}

	// proccess change
	if orderInput.Payment > productOrder.Price {
		change = orderInput.Payment - productOrder.Price
	}

	orderRsp := orderResponse{
		ObjectType:   oType,
		OrderRequest: *orderInput,
		Change:       change,
	}

	log.Printf("rsp %+v\n", orderRsp)
	rspBytes, err = json.Marshal(orderRsp)
	if err != nil {
		return shim.Error(err.Error())
	}

	stub.PutPrivateData(collection, orderInput.ID, rspBytes)

	rsp := struct {
		Rsp orderResponse
		Msg string
	}{
		Rsp: orderRsp,
		Msg: jsonResp,
	}
	log.Printf("rsp %+v\n", rsp)
	rspBytes, err = json.Marshal(rsp)
	if err != nil {
		return shim.Error(err.Error())
	}

	return shim.Success(rspBytes)
}

func (v *VirtualCoffeeShopChaincode) getOKOrders(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[getOKOrders] - START")
	defer log.Printf("[getOKOrders] - END")
	var collection string
	var r peer.Response
	var valAsbytes []byte
	var ok bool

	if len(args) > 0 {
		return shim.Error("Not expecting any arguments")
	}

	if r, collection, ok = getMSPCollectionName(stub); !ok {
		return r
	}

	if collection == "AllCollections" {
		//Bob's
		bolist := olist{}
		collection = "BobsCoffeeCollection"
		valAsbytes, r = getCollection(stub, collection, "OkTransactionDetails")
		if valAsbytes == nil {
			return r
		}
		err := json.Unmarshal(valAsbytes, &bolist)
		if err != nil {
			return shim.Error(err.Error())
		}
		bobs := shopRecord{
			Vendor: "BobsCoffees",
			Olist:  bolist,
		}

		//Charlie's
		colist := olist{}
		collection = "CharliesCoffeeCollection"
		valAsbytes, r = getCollection(stub, collection, "OkTransactionDetails")
		if valAsbytes == nil {
			return r
		}
		err = json.Unmarshal(valAsbytes, &colist)
		if err != nil {
			return shim.Error(err.Error())
		}
		charlies := shopRecord{
			Vendor: "CharliessCoffees",
			Olist:  colist,
		}

		//combine
		pAll := struct {
			Shops []shopRecord
		}{
			[]shopRecord{bobs, charlies},
		}
		log.Printf("pall: %+v\n", pAll)
		valAsbytes, err = json.Marshal(pAll)
		if err != nil {
			return shim.Error(err.Error())
		}
		log.Printf("valAsbytes-p: %+v\n", valAsbytes)
	} else {
		valAsbytes, r = getCollection(stub, collection, "OkTransactionDetails")
		if valAsbytes == nil {
			return r
		}
	}

	log.Printf("valAsbytes: %+v\n", valAsbytes)
	return shim.Success(valAsbytes)
}

func (v *VirtualCoffeeShopChaincode) getNOKOrders(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[getNOKOrders] - START")
	defer log.Printf("[getNOKOrders] - END")
	var collection string
	var r peer.Response
	var valAsbytes []byte
	var ok bool

	if len(args) > 0 {
		return shim.Error("Not expecting any arguments")
	}

	if r, collection, ok = getMSPCollectionName(stub); !ok {
		return r
	}

	if collection == "AllCollections" {
		//Bob's
		bolist := olist{}
		collection = "BobsCoffeeCollection"
		valAsbytes, r = getCollection(stub, collection, "NOKTransactionDetails")
		if valAsbytes == nil {
			return r
		}
		err := json.Unmarshal(valAsbytes, &bolist)
		if err != nil {
			return shim.Error(err.Error())
		}
		bobs := shopRecord{
			Vendor: "BobsCoffees",
			Olist:  bolist,
		}

		//Charlie's
		colist := olist{}
		collection = "CharliesCoffeeCollection"
		valAsbytes, r = getCollection(stub, collection, "NOKTransactionDetails")
		if valAsbytes == nil {
			return r
		}
		err = json.Unmarshal(valAsbytes, &colist)
		if err != nil {
			return shim.Error(err.Error())
		}
		charlies := shopRecord{
			Vendor: "CharliessCoffees",
			Olist:  colist,
		}

		//combine
		pAll := struct {
			Shops []shopRecord
		}{
			[]shopRecord{bobs, charlies},
		}
		log.Printf("pall: %+v\n", pAll)
		valAsbytes, err = json.Marshal(pAll)
		if err != nil {
			return shim.Error(err.Error())
		}
		log.Printf("valAsbytes-p: %+v\n", valAsbytes)
	} else {
		valAsbytes, r = getCollection(stub, collection, "NOKTransactionDetails")
		if valAsbytes == nil {
			return r
		}
	}

	log.Printf("valAsbytes: %+v\n", valAsbytes)
	return shim.Success(valAsbytes)
}

func (v *VirtualCoffeeShopChaincode) getTotalSales(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[getTotalSales] - START")
	defer log.Printf("[getTotalSales] - END")

	var collection string
	var r peer.Response
	var valAsbytes []byte
	var ok bool
	var tsales float32

	bobs := shopRecord{}
	charlies := shopRecord{}

	if len(args) > 0 {
		return shim.Error("Not expecting any arguments")
	}

	if r, collection, ok = getMSPCollectionName(stub); !ok {
		return r
	}

	if collection == "AllCollections" {
		//Bob's
		bolist := olist{}
		collection = "BobsCoffeeCollection"
		valAsbytes, r = getCollection(stub, collection, "OkTransactionDetails")
		if valAsbytes == nil {
			return r
		}
		err := json.Unmarshal(valAsbytes, &bolist)
		if err != nil {
			return shim.Error(err.Error())
		}

		for _, sale := range bolist {
			tsales += sale.Record.OrderRequest.Product.Price
		}

		bobs = shopRecord{
			Vendor: "BobsCoffees",
			Olist:  bolist,
			TSales: tsales,
		}

		//Charlie's
		colist := olist{}
		collection = "CharliesCoffeeCollection"
		valAsbytes, r = getCollection(stub, collection, "OkTransactionDetails")
		if valAsbytes == nil {
			return r
		}
		err = json.Unmarshal(valAsbytes, &colist)
		if err != nil {
			return shim.Error(err.Error())
		}

		for _, sale := range colist {
			tsales += sale.Record.OrderRequest.Product.Price
		}

		charlies = shopRecord{
			Vendor: "CharliessCoffees",
			Olist:  colist,
			TSales: tsales,
		}

		//combine
		pAll := struct {
			Shops []shopRecord
		}{
			[]shopRecord{bobs, charlies},
		}
		log.Printf("pall: %+v\n", pAll)
		valAsbytes, err = json.Marshal(pAll)
		if err != nil {
			return shim.Error(err.Error())
		}
	} else {
		list := olist{}
		valAsbytes, r = getCollection(stub, collection, "OkTransactionDetails")
		if valAsbytes == nil {
			return r
		}

		err := json.Unmarshal(valAsbytes, &list)
		if err != nil {
			return shim.Error(err.Error())
		}

		for _, sale := range list {
			tsales += sale.Record.OrderRequest.Product.Price
		}

		shop := shopRecord{
			Olist:  list,
			TSales: tsales,
		}

		log.Printf("shop: %+v\n", shop)
		valAsbytes, err = json.Marshal(shop)
		if err != nil {
			return shim.Error(err.Error())
		}
	}
	return shim.Success(valAsbytes)
}

func (v *VirtualCoffeeShopChaincode) getBobsTotalSales(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[getBobsTotalSales] - START")
	defer log.Printf("[getBobsTotalSales] - END")

	var collection string
	var r peer.Response
	var valAsbytes []byte
	var tsales float32

	if len(args) > 0 {
		return shim.Error("Not expecting any arguments")
	}

	list := olist{}
	collection = "BobsCoffeeCollection"
	valAsbytes, r = getCollection(stub, collection, "OkTransactionDetails")
	if valAsbytes == nil {
		return r
	}

	err := json.Unmarshal(valAsbytes, &list)
	if err != nil {
		return shim.Error(err.Error())
	}

	for _, sale := range list {
		tsales += sale.Record.OrderRequest.Product.Price
	}

	shop := shopRecord{
		Olist:  list,
		TSales: tsales,
	}

	log.Printf("shop: %+v\n", shop)
	valAsbytes, err = json.Marshal(shop)
	if err != nil {
		return shim.Error(err.Error())
	}

	return shim.Success(valAsbytes)
}

func (v *VirtualCoffeeShopChaincode) getCharliesTotalSales(stub shim.ChaincodeStubInterface, args []string) peer.Response {
	log.Printf("[getCharliesTotalSales] - START")
	defer log.Printf("[getCharliesTotalSales] - END")

	var collection string
	var r peer.Response
	var valAsbytes []byte
	var tsales float32

	if len(args) > 0 {
		return shim.Error("Not expecting any arguments")
	}

	list := olist{}
	collection = "CharliesCoffeeCollection"
	valAsbytes, r = getCollection(stub, collection, "OkTransactionDetails")
	if valAsbytes == nil {
		return r
	}

	err := json.Unmarshal(valAsbytes, &list)
	if err != nil {
		return shim.Error(err.Error())
	}

	for _, sale := range list {
		tsales += sale.Record.OrderRequest.Product.Price
	}

	shop := shopRecord{
		Olist:  list,
		TSales: tsales,
	}

	log.Printf("shop: %+v\n", shop)
	valAsbytes, err = json.Marshal(shop)
	if err != nil {
		return shim.Error(err.Error())
	}

	return shim.Success(valAsbytes)
}

func sanitizeProduct(stub shim.ChaincodeStubInterface, args []string, transMap map[string][]byte) (peer.Response, *productTransientInput, bool) {
	if len(transMap["product"]) == 0 {
		return shim.Error("product value in the transient map must be a non-empty JSON string"), nil, false
	}

	var productInput productTransientInput
	err := json.Unmarshal(transMap["product"], &productInput)
	if err != nil {
		return shim.Error("Failed to decode JSON of: " + string(transMap["product"])), nil, false
	}

	if len(productInput.Name) == 0 {
		return shim.Error("name field must be a non-empty string"), nil, false
	}
	if len(productInput.Type) == 0 {
		return shim.Error("color field must be a non-empty string"), nil, false
	}
	if productInput.Price <= 0 {
		return shim.Error("size field must be a positive integer"), nil, false
	}

	return peer.Response{}, &productInput, true
}

func sanitizeOrder(stub shim.ChaincodeStubInterface, args []string, transMap map[string][]byte) (peer.Response, *orderRequest, bool) {
	if len(transMap["order"]) == 0 {
		return shim.Error("order value in the transient map must be a non-empty JSON string"), nil, false
	}

	var orderInput orderRequest
	log.Printf("input string %s\n", string(transMap["order"]))
	err := json.Unmarshal(transMap["order"], &orderInput)
	if err != nil {
		return shim.Error("Failed to decode JSON of: " + string(transMap["order"])), nil, false
	}

	if len(orderInput.ID) == 0 {
		return shim.Error("ID field must be a non-empty string"), nil, false
	}
	if len(orderInput.Name) == 0 {
		return shim.Error("Name field must be a non-empty string"), nil, false
	}
	if len(orderInput.Vendor) == 0 {
		return shim.Error("Vendor field must be a non-empty string"), nil, false
	}
	if (productTransientInput{}) == orderInput.Product {
		return shim.Error("Product field must not be empty"), nil, false
	}
	if 0 >= orderInput.Qty {
		return shim.Error("Qty field must be greater than 0"), nil, false
	}

	return peer.Response{}, &orderInput, true
}

func sanitizeInput(stub shim.ChaincodeStubInterface, args []string) (peer.Response, interface{}, bool) {
	if len(args) != 0 {
		return shim.Error("Incorrect number of arguments. Private product data must be passed in transient map."), nil, false
	}

	transMap, err := stub.GetTransient()
	if err != nil {
		return shim.Error("Error getting transient: " + err.Error()), nil, false
	}

	if _, ok := transMap["product"]; ok {
		return sanitizeProduct(stub, args, transMap)
	}

	if _, ok := transMap["order"]; ok {
		return sanitizeOrder(stub, args, transMap)
	}

	return shim.Error("Error getting transient: " + err.Error()), nil, false
}

func getMSPCollectionName(stub shim.ChaincodeStubInterface) (peer.Response, string, bool) {
	var collection string
	mspid, err := cid.GetMSPID(stub)

	if err != nil {
		return shim.Error("Unable to get mspid of sender"), collection, false
	}

	switch mspid {
	case "bobscoffeeMSP":
		collection = "BobsCoffeeCollection"
	case "charliescoffeeMSP":
		collection = "CharliesCoffeeCollection"
	case "coffeeauditMSP":
		collection = "AllCollections"
	default:
		return shim.Error("Unknown mspid: " + mspid), collection, false
	}

	return peer.Response{}, collection, true
}

func checkIfProductExists(stub shim.ChaincodeStubInterface, product *productTransientInput) ([]byte, string, bool, error) {
	var collection string
	var ok bool

	var err error

	if _, collection, ok = getMSPCollectionName(stub); !ok {
		return nil, collection, false, nil
	}

	log.Println("collection: " + collection)
	log.Println("product.Name: " + product.Name)

	productAsBytes, err := stub.GetPrivateData(collection, product.Name)
	if err != nil {
		return nil, collection, false, err
	}

	if productAsBytes != nil {
		return productAsBytes, collection, true, nil
	}

	return productAsBytes, collection, false, nil
}

func createProductPrivateDetails(stub shim.ChaincodeStubInterface, productInput *productTransientInput, collection string) peer.Response {
	productPrivateDetails := &product{
		ObjectType: "productPrivateDetails",
		Name:       productInput.Name,
		Type:       productInput.Type,
		Price:      productInput.Price,
	}

	log.Printf("productPrivateDetails: %+v", productPrivateDetails)

	productPrivateDetailsBytes, err := json.Marshal(productPrivateDetails)
	if err != nil {
		return shim.Error(err.Error())
	}

	log.Println("collection: " + collection)
	log.Println("productPrivateDetails.Name: " + productPrivateDetails.Name)

	err = stub.PutPrivateData(collection, productInput.Name, productPrivateDetailsBytes)
	if err != nil {
		return shim.Error(err.Error())
	}

	return shim.Success(nil)
}

func getCollection(stub shim.ChaincodeStubInterface, collection string, colType string) ([]byte, peer.Response) {
	var jsonResp string

	qstring := "{\"selector\":{\"docType\":\"" + colType + "\"}}"

	valAsbytes, err := getQueryResultForQueryString(stub, collection, qstring)
	if err != nil {
		log.Printf("Error: %s\n", err.Error())
		jsonResp = "{\"Error\":\"Failed to get state\"}"
		return nil, shim.Error(jsonResp + err.Error())
	} else if valAsbytes == nil {
		jsonResp = "{\"Error\":\"No products\"}"
		return nil, shim.Error(jsonResp)
	}
	return valAsbytes, peer.Response{}
}

func getQueryResultForQueryString(stub shim.ChaincodeStubInterface, collection string, queryString string) ([]byte, error) {

	fmt.Printf("- getQueryResultForQueryString queryString:\n%s\n", queryString)
	resultsIterator, err := stub.GetPrivateDataQueryResult(collection, queryString)
	if err != nil {
		return nil, err
	}
	defer resultsIterator.Close()

	// buffer is a JSON array containing QueryRecords
	var buffer bytes.Buffer
	buffer.WriteString("[")

	bArrayMemberAlreadyWritten := false
	for resultsIterator.HasNext() {
		queryResponse, err := resultsIterator.Next()
		if err != nil {
			return nil, err
		}
		// Add a comma before array members, suppress it for the first array member
		if bArrayMemberAlreadyWritten == true {
			buffer.WriteString(",")
		}
		buffer.WriteString("{\"Key\":")
		buffer.WriteString("\"")
		buffer.WriteString(queryResponse.Key)
		buffer.WriteString("\"")

		buffer.WriteString(", \"Record\":")
		// Record is a JSON object, so we write as-is
		buffer.WriteString(string(queryResponse.Value))
		buffer.WriteString("}")
		bArrayMemberAlreadyWritten = true
	}
	buffer.WriteString("]")

	fmt.Printf("- getQueryResultForQueryString queryResult:\n%s\n", buffer.String())

	return buffer.Bytes(), nil
}
