DefinitionBlock ("", "SSDT", 2, "TXGFAN", "FANMODE", 0x00000001)
{
    External (\_SB.ATKD, DeviceObj)
    External (\_SB.PC00.LPCB.EC0.FANM, FieldUnitObj)
    Scope (\_SB.ATKD)
    {
        // Read the existing firmware field without storing to EC.
        Method (TGFM, 0, Serialized)
        {
            Return (\_SB.PC00.LPCB.EC0.FANM)
        }
    }
}
