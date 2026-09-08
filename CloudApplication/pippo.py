# nurse_mock.py
import asyncio
from aiocoap import Context, Message, resource

class Association(resource.Resource):
    async def render_post(self, request):
        print("Ricevuto POST /er/patient/association:")
        print(request.payload.decode())
        return Message(code=65)  # 2.05 Content

class Called(resource.Resource):
    async def render_put(self, request):
        print("Ricevuto PUT /er/patient/called")
        return Message(code=68)  # 2.04 Changed

class TriageUpdate(resource.Resource):
    async def render_put(self, request):
        print("Ricevuto PUT /er/patient/triage:", request.payload.decode())
        return Message(code=68)

class Discharge(resource.Resource):
    async def render_put(self, request):
        print("Ricevuto PUT /er/patient/discharge:", request.payload.decode() if request.payload else "(vuoto)")
        return Message(code=68)

async def main():
    root = resource.Site()
    root.add_resource(['er', 'patient', 'association'], Association())
    root.add_resource(['er', 'patient', 'called'], Called())
    root.add_resource(['er', 'patient', 'triage'], TriageUpdate())
    root.add_resource(['er', 'patient', 'discharge'], Discharge())
    await Context.create_server_context(root, bind=('::1', 5683))
    await asyncio.get_running_loop().create_future()




asyncio.run(main())
