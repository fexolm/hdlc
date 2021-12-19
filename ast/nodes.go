package ast

type Package struct {
	Name    string
	Imports []string
	Chips   []*Chip
}

func NewPackage(name string, imports []string, chips []*Chip) *Package {
	return &Package{Name: name, Imports: imports, Chips: chips}
}

type Type string

type Value struct {
	Name string
	Typ Type
}

type ChipSignarure struct {
	Inputs  []*Value
	Outputs []string // TODO: []Type 
}

type ChipOp struct {
	ChipName string
	Args  []*Value
	Results []*Value
}

type ChipBody struct {
	Chips   []*ChipOp
	Results []*Value
}

type Chip struct {
	Name      string
	Interface *ChipSignarure
	Impl      *ChipBody
}
