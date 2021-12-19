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

type Param struct {
	Name string
	Typ Type
}

type Wire struct {
	Name string
	Typ Type
}

type ChipSignarure struct {
	Inputs  []*Param
	Outputs []*Param
}

type ChipOp struct {
	ChipName string
	Args  []*Wire
	Results []*Wire
}

type ChipBody struct {
	Chips   []*ChipOp
	Results []*Wire
}

type Chip struct {
	Name      string
	Interface *ChipSignarure
	Impl      *ChipBody
}
