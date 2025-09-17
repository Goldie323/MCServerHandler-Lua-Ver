local MyLib = require("platform")

MCServerHandler = {}
MCServerHandler.__index = MCServerHandler

local function ValidateFolder(Folder)
    -- Input validation
    if not Folder then
        return false, "Folder path cannot be nil"
    end
    
    if type(Folder) ~= "string" then
        return false, "Folder path must be a string"
    end
    
    if Folder == "" then
        return false, "Folder path cannot be empty"
    end
    
    -- Check if directory exists
    if not MyLib.isDir(Folder) then
        return false, "Directory doesn't exist or is not accessible: " .. Folder
    end

    return true
end

function MCServerHandler:new()
    local StartData = {
        WorldSrcDir = "",
        ServerSrcDir = "",
        WorldWrkDir = "",
        ServerWrkDir = ""
    }
    setmetatable(StartData, self)
    return StartData
end

function MCServerHandler:SetWorldSrc(Folder)
    local success, error_msg = ValidateFolder(Folder)

    if not success then
        return false, error_msg
    end
    
    self.WorldSrcDir = Folder
    return true
end

function MCServerHandler:GetWorldSrc()
    return self.WorldSrcDir
end

function MCServerHandler:SetServerSrc(Folder)
    local success, error_msg = ValidateFolder(Folder)

    if not success then
        return false, error_msg
    end
    
    self.ServerSrcDir = Folder
    return true
end

function MCServerHandler:GetServerSrc()
    return self.ServerSrcDir
end

function MCServerHandler:SetWorldWrkDir(Folder, ForceFull)
    local success, error_msg = ValidateFolder(Folder)

    if not success then
        return false, error_msg
    end

    if ForceFull then
        self.WorldWrkDir = Folder
        return true, "Directory was forced to use even if full"
    end

    if not MyLib.isDirEmpty(Folder) then
        return false, "Directory must be empty"
    end
    
    self.WorldWrkDir = Folder
    return true
end

function MCServerHandler:GetWorldWrkDir()
    return self.WorldWrkDir
end

function MCServerHandler:SetServerWrkDir(Folder, ForceFull)
    local success, error_msg = ValidateFolder(Folder)

    if not success then
        return false, error_msg
    end

    if ForceFull then
        self.ServerWrkDir = Folder
        return true, "Directory was forced to use even if full"
    end

    if not MyLib.isDirEmpty(Folder) then
        return false, "Directory must be empty"
    end
    
    self.ServerWrkDir = Folder
    return true
end

function MCServerHandler:GetServerWrkDir()
    return self.ServerWrkDir
end


local folder = "/home/goldie323/Coding Project/MCServerHandler Lua-ver/"

local NewServer = MCServerHandler:new()

local success, message = NewServer:SetServerWrkDir(folder, true)

print(success, message)

print(NewServer:GetServerWrkDir())
