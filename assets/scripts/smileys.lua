if not env.register_smileys then
	return
end

local listing = {
	{
		description = "Normal"
	},
	{
		description = "Y2000 Glasses"
	},
	{
		description = "Silly Face"
	},
	{
		description = "Kool"
	},
	{
		description = "Fedora"
	}
}

env.register_smileys({
	name = "basic"
}, listing)
