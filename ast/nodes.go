package ast

type Package struct {
	Name    string
	Imports []string
	Chips   []*Chip
}

func NewPackage(name string, imports []string, chips []*Chip) *Package {
	return &Package{Name: name, Imports: imports, Chips: chips}
}

type Param struct {
	Name string
	Type string
}

type ChipSignarure struct {
	Inputs  []*Param
	Outputs []*Param
}

type ChipOp struct {
	Results  []string
	ChipName string
	Params   []string
}

type ChipBody struct {
	Chips   []*ChipOp
	Results []string
}

type Chip struct {
	Name      string
	Interface *ChipSignarure
	Impl      *ChipBody
}
