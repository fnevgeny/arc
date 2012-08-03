"""
resource to fetch a bloom filter from
"""

from twisted.python import log
from twisted.web import resource



class CacheResource(resource.Resource):

    isLeaf = True

    def __init__(self, cache_service):
        resource.Resource.__init__(self)
        self.cache_service = cache_service


    def render_GET(self, request):
        client = request.getClient() + "/" + request.getClientIP()
        log.msg("GET request on cache from %s" % client)

        gen_time, hashes, cache = self.cache_service.getCache()

        request.setHeader('Content-type', 'application/vnd.org.ndgf.acix.bloomfilter')
        request.setHeader('Content-length', str(len(cache)))

        request.setHeader("x-hashes", ','.join(hashes))
        request.setHeader("x-cache-time", str(gen_time))

        return cache

