package ir

type ChipType struct {
	Name        string
	InputsSize  int
	OutputsSize int
}

type Chip struct {
	Type *ChipType
	Impl *ChipImpl
}

type Module struct {
	Chips []*Chip
}

type ChipImpl struct {
	Chips []*ChipInstance
}

type ChipInstance struct {
	Inputs  []*ChipInstance
	Outputs []*ChipInstance
	Type    *ChipType
}
